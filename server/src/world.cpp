#include "world.h"

namespace saide {

void World::addPlayer(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_[id] = Player{id, 0, 0, 0, 0};
}

void World::removePlayer(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_.erase(id);
}

bool World::hasPlayer(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return players_.find(id) != players_.end();
}

bool World::requestMove(const std::string& id, int x, int y) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = players_.find(id);
    if (it == players_.end() || x < 0 || x >= kWorldWidth || y < 0 || y >= kWorldHeight) {
        return false;
    }
    it->second.destinationX = x;
    it->second.destinationY = y;
    return true;
}

std::vector<Player> World::advancePlayers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Player> moved;
    for (auto& [id, player] : players_) {
        if (player.x < player.destinationX) {
            ++player.x;
        } else if (player.x > player.destinationX) {
            --player.x;
        } else if (player.y < player.destinationY) {
            ++player.y;
        } else if (player.y > player.destinationY) {
            --player.y;
        } else {
            continue;
        }
        moved.push_back(player);
    }
    return moved;
}

std::vector<Player> World::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Player> result;
    result.reserve(players_.size());
    for (const auto& [id, player] : players_) {
        result.push_back(player);
    }
    return result;
}

} // namespace saide
