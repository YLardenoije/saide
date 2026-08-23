#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace saide {

struct Player {
    std::string id;
    int x = 0;
    int y = 0;
    int destinationX = 0;
    int destinationY = 0;
};

constexpr int kWorldWidth = 100;
constexpr int kWorldHeight = 100;

// Holds authoritative world state. Safe to call from the network thread(s)
// and the tick thread concurrently.
class World {
public:
    void addPlayer(const std::string& id);
    void removePlayer(const std::string& id);
    bool hasPlayer(const std::string& id) const;

    // Sets a destination inside the world. Movement toward it remains server-owned.
    bool requestMove(const std::string& id, int x, int y);

    // Advances every moving player by one orthogonal tile and returns those moved.
    std::vector<Player> advancePlayers();

    std::vector<Player> snapshot() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Player> players_;
};

} // namespace saide
