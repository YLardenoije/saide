"""Integration smoke test for the baseline chat milestone.

Starts the built C++ server, connects two lightweight WebSocket clients,
sends a chat message from one client, and verifies the other receives the
server-broadcast chat event.
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
CHAT_TIMEOUT_SECS = 2.0


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
    while True:
        message = json.loads(await websocket.recv())
        if message["type"] == "HELLO_ACK":
            assert message["accepted"] is True
            return message["id"]


async def _wait_for_chat_broadcast(
    websocket: websockets.WebSocketClientProtocol,
    sender_id: str,
    expected_text: str,
) -> None:
    deadline = time.monotonic() + CHAT_TIMEOUT_SECS
    while time.monotonic() < deadline:
        message = json.loads(await websocket.recv())
        if (
            message.get("type") == "CHAT_BROADCAST"
            and message.get("from") == sender_id
            and message.get("text") == expected_text
        ):
            return
    raise AssertionError(f"did not observe CHAT_BROADCAST from {sender_id} in time")


async def _run_chat_test() -> None:
    async with websockets.connect(SERVER_URL) as client_a, websockets.connect(SERVER_URL) as client_b:
        id_a = await _hello(client_a)
        _ = await _hello(client_b)

        chat_text = "hello from integration test"
        await client_a.send(json.dumps({"type": "CHAT_SEND", "text": chat_text}))

        await _wait_for_chat_broadcast(client_b, id_a, chat_text)


def test_second_client_receives_first_client_chat(server: None) -> None:
    asyncio.run(_run_chat_test())


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
