#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <thread>

#include "world.h"

using json = nlohmann::json;

namespace {

constexpr int kPort = 43594;
constexpr int kTicksPerSecond = 5;
constexpr int kProtocolVersion = 1;

json playerSpawnMessage(const rs::Player& player) {
    return json{{"type", "PLAYER_SPAWN"}, {"id", player.id}, {"x", player.x}, {"y", player.y}};
}

json playerMovedMessage(const rs::Player& player) {
    return json{{"type", "PLAYER_MOVED"}, {"id", player.id}, {"x", player.x}, {"y", player.y}};
}

} // namespace

int main() {
    ix::initNetSystem();

    rs::World world;
    ix::WebSocketServer server(kPort, "0.0.0.0");

    server.setOnClientMessageCallback(
        [&world, &server](std::shared_ptr<ix::ConnectionState> connectionState,
                           ix::WebSocket& webSocket,
                           const ix::WebSocketMessagePtr& msg) {
            const std::string& id = connectionState->getId();

            if (msg->type == ix::WebSocketMessageType::Close) {
                if (world.hasPlayer(id)) {
                    world.removePlayer(id);
                    std::cout << "Player disconnected: " << id << std::endl;

                    const std::string payload = json{{"type", "PLAYER_DESPAWN"}, {"id", id}}.dump();
                    for (const auto& client : server.getClients()) {
                        if (client->getReadyState() == ix::ReadyState::Open) {
                            client->send(payload);
                        }
                    }
                }
                return;
            }

            if (msg->type != ix::WebSocketMessageType::Message) {
                return;
            }

            json command;
            try {
                command = json::parse(msg->str);
            } catch (const json::exception& e) {
                std::cerr << "Malformed message from " << id << ": " << e.what() << std::endl;
                return;
            }

            const std::string type = command.value("type", "");

            if (type == "HELLO") {
                if (world.hasPlayer(id)) {
                    return; // already handshaked, ignore duplicate HELLO
                }

                const int clientVersion = command.value("protocol_version", -1);
                if (clientVersion != kProtocolVersion) {
                    webSocket.send(json{{"type", "HELLO_ACK"},
                                         {"accepted", false},
                                         {"reason", "unsupported protocol_version"}}
                                       .dump());
                    webSocket.close();
                    return;
                }

                // Tell the new player about everyone already in the world before
                // adding it, so it doesn't receive its own spawn twice.
                for (const auto& existing : world.snapshot()) {
                    webSocket.send(playerSpawnMessage(existing).dump());
                }

                world.addPlayer(id);
                std::cout << "Player connected: " << id << std::endl;

                webSocket.send(json{{"type", "HELLO_ACK"},
                                     {"accepted", true},
                                     {"protocol_version", kProtocolVersion},
                                     {"id", id}}
                                   .dump());

                const std::string spawnPayload = playerSpawnMessage(rs::Player{id, 0.0, 0.0}).dump();
                for (const auto& client : server.getClients()) {
                    if (client->getReadyState() == ix::ReadyState::Open) {
                        client->send(spawnPayload);
                    }
                }
            } else if (type == "MOVE_REQUEST") {
                if (!world.hasPlayer(id)) {
                    return; // must complete HELLO handshake first
                }
                try {
                    world.requestMove(
                        id, command.at("x").get<double>(), command.at("y").get<double>());
                } catch (const json::exception& e) {
                    std::cerr << "Bad MOVE_REQUEST from " << id << ": " << e.what() << std::endl;
                }
            }
        });

    const auto listenResult = server.listen();
    if (!listenResult.first) {
        std::cerr << "Failed to listen on port " << kPort << ": " << listenResult.second
                   << std::endl;
        return 1;
    }

    server.start();
    std::cout << "runescape_server listening on port " << kPort << " (" << kTicksPerSecond
               << " ticks/sec)" << std::endl;

    const auto tickDuration = std::chrono::milliseconds(1000 / kTicksPerSecond);
    while (true) {
        const auto tickStart = std::chrono::steady_clock::now();

        // send_updates(): broadcast each player's current position every tick.
        for (const auto& player : world.snapshot()) {
            const std::string payload = playerMovedMessage(player).dump();
            for (const auto& client : server.getClients()) {
                if (client->getReadyState() == ix::ReadyState::Open) {
                    client->send(payload);
                }
            }
        }

        std::this_thread::sleep_until(tickStart + tickDuration);
    }
}
