#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace saide {

struct Player {
    std::string id;
    double x = 0.0;
    double y = 0.0;
};

// Holds authoritative world state. Safe to call from the network thread(s)
// and the tick thread concurrently.
class World {
public:
    void addPlayer(const std::string& id);
    void removePlayer(const std::string& id);
    bool hasPlayer(const std::string& id) const;

    // Records the player's requested position. No validation yet - this is
    // the minimal movement milestone, range/speed checks come later.
    void requestMove(const std::string& id, double x, double y);

    std::vector<Player> snapshot() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Player> players_;
};

} // namespace saide
