#pragma once
#include <vector>

#include "entity.hpp"

namespace rw_ecs {

class World;

struct Commands {
    World& world;

    explicit Commands(World& w) : world(w) {}

    template <typename... Components>
    void create_entity(Components&&... components);

    template <typename... Components>
    void create_entities(size_t n);

    template <typename... Components, typename Callback>
    void create_entities(size_t n, Callback&& callback);

    void destroy_entity(Entity e);

    void destroy_entities(const std::vector<Entity>& entities);

    template <typename Component>
    void add_component(Entity e);

    template <typename Component>
    void add_component(Entity e, Component&& c);

    template <typename Component>
    void add_component(const std::vector<Entity>& entities);

    template <typename Component>
    void add_component(const std::vector<Entity>& entities, Component&& c);

    template <typename Component>
    void add_component(const std::vector<Entity>& entities, const std::vector<Component>& components);

    template <typename Component>
    void remove_component(Entity e);

    template <typename Component>
    void remove_component(const std::vector<Entity>& entities);
};

}; // namespace rw_ecs