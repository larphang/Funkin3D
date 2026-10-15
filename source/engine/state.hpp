
#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>

namespace engine {

class EngineContext;

namespace detail {

template<typename T, typename = void>
struct HasRender : std::false_type {};

template<typename T>
struct HasRender<T, std::void_t<decltype(std::declval<T&>().render())>> : std::true_type {};

template<typename T, typename = void>
struct HasZOrder : std::false_type {};

template<typename T>
struct HasZOrder<T, std::void_t<decltype(std::declval<T&>().zorder)>> : std::true_type {};

template<typename T>
void renderObject(T& object) {
    if constexpr (HasRender<T>::value)
        object.render();
}

template<typename T>
float getZOrder(const T& object) {
    if constexpr (HasZOrder<T>::value)
        return static_cast<float>(object.zorder);

    return 0.0f;
}

}

class State {
public:
    bool allowScripting = false;
    std::string state_name = "unnamed";
    bool autoSort = true;

    template<typename T>
    void add(std::shared_ptr<T> member, bool sort = false) {
        addTop(std::move(member), sort);
    }

    template<typename T>
    void addTop(std::shared_ptr<T> member, bool sort = false) {
        addMember(topMembers, std::move(member), sort);
    }

    template<typename T>
    void addBottom(std::shared_ptr<T> member, bool sort = false) {
        addMember(bottomMembers, std::move(member), sort);
    }

    template<typename T>
    void removeMember(const std::shared_ptr<T>& member) {
        removeFrom(topMembers, member);
        removeFrom(bottomMembers, member);
    }

    template<typename T>
    void remove(const std::shared_ptr<T>& member) {
        removeMember(member);
    }

    virtual ~State() = default;

    virtual void enter([[maybe_unused]] EngineContext& ctx) {}
    virtual void update([[maybe_unused]] EngineContext& ctx, [[maybe_unused]] float dt) = 0;

    virtual void renderTop([[maybe_unused]] EngineContext& ctx) {
        renderMembers(topMembers);
    }
    virtual void renderBottom([[maybe_unused]] EngineContext& ctx) {
        renderMembers(bottomMembers);
    }

    virtual void render(EngineContext& ctx) {
        renderTop(ctx);
    }

    virtual void onResize([[maybe_unused]] EngineContext& ctx, [[maybe_unused]] int width, [[maybe_unused]] int height) {}
    virtual void leave([[maybe_unused]] EngineContext& ctx) {}

private:
    struct Member {
        std::shared_ptr<void> owner;
        void* object;
        void (*render)(void*);
        float (*zorder)(const void*);

        Member(std::shared_ptr<void> ptr, void (*renderFn)(void*), float (*zorderFn)(const void*))
            : owner(std::move(ptr)),
              object(owner.get()),
              render(renderFn),
              zorder(zorderFn) {}
    };

    std::vector<Member> topMembers;
    std::vector<Member> bottomMembers;

    template<typename T>
    void addMember(std::vector<Member>& members, std::shared_ptr<T> member, bool sort) {
        using Object = std::remove_const_t<T>;

        members.emplace_back(
            std::move(member),
            [](void* ptr) {
                detail::renderObject(*static_cast<Object*>(ptr));
            },
            [](const void* ptr) -> float {
                return detail::getZOrder(
                    *static_cast<const Object*>(ptr)
                );
            }
        );

        if (autoSort || sort)
            sortMembers(members);
    }

    template<typename T>
    void removeFrom(std::vector<Member>& members, const std::shared_ptr<T>& member) {
        members.erase(
            std::remove_if(members.begin(), members.end(),
                [&member](const Member& item) {
                    return item.object == member.get();
                }
            ),
            members.end()
        );
    }

    static void renderMembers(std::vector<Member>& members) {
        for (auto& member : members)
            member.render(member.object);
    }

    static void sortMembers(std::vector<Member>& members) {
        std::stable_sort(members.begin(), members.end(),
            [](const Member& a, const Member& b) {
                return a.zorder(a.object) < b.zorder(b.object);
            }
        );
    }
};

}
