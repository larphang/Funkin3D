#pragma once
#include "basic.hpp"
#include <algorithm>

namespace Core {
namespace Objects {

class Group : public Basic {
int size = -1;
public:
    Group(int size = -1) {
        this->size = size;
    }
    ~Group() {
    }
    bool visible = true;
    std::vector<std::shared_ptr<Basic>> members;

    void add(std::shared_ptr<Basic> obj, bool resort = true) {
        if (size != -1 && static_cast<int>(members.size()) >= size) {
            return;
        }
        members.push_back(obj);
        if (resort) {
            std::sort(members.begin(), members.end(), [](const std::shared_ptr<Basic>& a, const std::shared_ptr<Basic>& b) {
                return a->zorder < b->zorder;
            });
        }
    }

    void remove(std::shared_ptr<Basic> obj) {
        members.erase(std::remove(members.begin(), members.end(), obj), members.end());
    }

    void clear() {
        members.clear();
    }

    void shrink_to_fit() {
        members.shrink_to_fit();
    }

    void update(float dt) override {
        for (auto& member : members) {
            member->update(dt);
        }
    }

    bool contains(std::shared_ptr<Basic> obj) {
        return std::find(members.begin(), members.end(), obj) != members.end();
    }

    void render() override {
        if (!visible) return;
        for (auto& member : members) {
            member->render();
        }
    }

    std::shared_ptr<Basic> get(int index) {
        if (index < 1 || index > static_cast<int>(members.size())) {
            return nullptr;
        }
        return members[index - 1];
    }
};

} // namespace Objects
} // namespace Core
