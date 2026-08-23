#include "world.h"

namespace saide {

void World::addPlayer(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_[id] = Player{id, 0.0, 0.0};
}

void World::removePlayer(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_.erase(id);
}

bool World::hasPlayer(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return players_.find(id) != players_.end();
}

void World::requestMove(const std::string& id, double x, double y) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = players_.find(id);
    if (it != players_.end()) {
        it->second.x = x;
        it->second.y = y;
    }
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
