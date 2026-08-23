"""Integration smoke test for the first multiplayer milestone.

Starts the built C++ server, connects two lightweight WebSocket clients,
moves one, and asserts the other observes the movement. Matches the
non-graphical integration test flow described in docs/NETWORKING.adoc.
"""
from __future__ import annotations

import asyncio
import json
import subprocess
import sys
import time
from pathlib import Path

import pytest
import websockets

_EXE_NAME = "saide_server.exe" if sys.platform == "win32" else "saide_server"
SERVER_EXE = Path(__file__).resolve().parents[1] / "server" / "build" / _EXE_NAME
SERVER_URL = "ws://127.0.0.1:43594"
PROTOCOL_VERSION = 1
STARTUP_TIMEOUT_SECS = 5.0
MOVE_TIMEOUT_SECS = 2.0


@pytest.fixture()
def server() -> subprocess.Popen[str]:
    if not SERVER_EXE.exists():
        pytest.skip(f"server executable not built: {SERVER_EXE}")

    process = subprocess.Popen(
        [str(SERVER_EXE)],
        cwd=SERVER_EXE.parent,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        deadline = time.monotonic() + STARTUP_TIMEOUT_SECS
        while time.monotonic() < deadline:
            line = process.stdout.readline() if process.stdout else ""
            if "listening on port" in line:
                break
        else:
            process.terminate()
            raise RuntimeError("server did not report listening in time")
        yield process
    finally:
        process.terminate()
        process.wait(timeout=5)


async def _hello(websocket: websockets.WebSocketClientProtocol) -> str:
    await websocket.send(json.dumps({"type": "HELLO", "protocol_version": PROTOCOL_VERSION}))
    # The server may send PLAYER_SPAWN for already-connected players before
    # our own HELLO_ACK; skip those while waiting for the handshake reply.
    while True:
        message = json.loads(await websocket.recv())
        if message["type"] == "HELLO_ACK":
            assert message["accepted"] is True
            return message["id"]


async def _run_movement_test() -> None:
    async with websockets.connect(SERVER_URL) as client_a, websockets.connect(SERVER_URL) as client_b:
        id_a = await _hello(client_a)
        id_b = await _hello(client_b)
        assert id_a != id_b

        target_x, target_y = 3, 2
        await client_a.send(json.dumps({"type": "MOVE_REQUEST", "x": target_x, "y": target_y}))

        async def collect_path(websocket: websockets.WebSocketClientProtocol) -> list[tuple[int, int]]:
            path: list[tuple[int, int]] = []
            deadline = time.monotonic() + MOVE_TIMEOUT_SECS
            while len(path) < 5:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise AssertionError(f"did not observe complete path for {id_a}: {path}")
                message = json.loads(await asyncio.wait_for(websocket.recv(), remaining))
                if (
                    message.get("type") == "PLAYER_MOVED"
                    and message.get("id") == id_a
                ):
                    path.append((message["x"], message["y"]))
            return path

        # One orthogonal tile is traversed per server tick until the destination.
        assert await collect_path(client_b) == [(1, 0), (2, 0), (3, 0), (3, 1), (3, 2)]

        # Out-of-bounds destinations are ignored and cannot move the player.
        await client_a.send(json.dumps({"type": "MOVE_REQUEST", "x": 100, "y": 2}))
        with pytest.raises(asyncio.TimeoutError):
            while True:
                message = json.loads(await asyncio.wait_for(client_b.recv(), 0.35))
                if message.get("type") == "PLAYER_MOVED" and message.get("id") == id_a:
                    raise AssertionError("out-of-bounds destination moved the player")


async def _abort_connected_client() -> None:
    websocket = await websockets.connect(SERVER_URL)
    await _hello(websocket)
    websocket.transport.abort()
    await asyncio.sleep(0.25)


async def _observe_client_disconnect() -> None:
    async with websockets.connect(SERVER_URL) as observer:
        await _hello(observer)
        disconnected = await websockets.connect(SERVER_URL)
        disconnected_id = await _hello(disconnected)
        await disconnected.close()

        deadline = time.monotonic() + MOVE_TIMEOUT_SECS
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise AssertionError(f"did not observe PLAYER_DESPAWN for {disconnected_id}")
            message = json.loads(await asyncio.wait_for(observer.recv(), remaining))
            if (
                message.get("type") == "PLAYER_DESPAWN"
                and message.get("id") == disconnected_id
            ):
                return


def test_second_client_sees_first_client_move(server: subprocess.Popen[str]) -> None:
    asyncio.run(_run_movement_test())
    asyncio.run(_observe_client_disconnect())
    asyncio.run(_abort_connected_client())
    time.sleep(0.25)
    assert server.poll() is None, "server exited after a client disconnected"


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
