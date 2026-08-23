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
def server() -> None:
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
        yield
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

        target_x, target_y = 123.0, 45.0
        await client_a.send(json.dumps({"type": "MOVE_REQUEST", "x": target_x, "y": target_y}))

        async def wait_for_move(websocket: websockets.WebSocketClientProtocol) -> None:
            deadline = time.monotonic() + MOVE_TIMEOUT_SECS
            while time.monotonic() < deadline:
                message = json.loads(await websocket.recv())
                if (
                    message.get("type") == "PLAYER_MOVED"
                    and message.get("id") == id_a
                    and message.get("x") == target_x
                    and message.get("y") == target_y
                ):
                    return
            raise AssertionError(f"did not observe PLAYER_MOVED for {id_a} in time")

        # Client B (a different connection) must see client A's movement.
        await wait_for_move(client_b)


def test_second_client_sees_first_client_move(server: None) -> None:
    asyncio.run(_run_movement_test())


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
