#pragma once
#include <exception>
#include <iostream>

#include <ecs/world.hpp>

using namespace rw_ecs;

template <typename Func>
inline bool test(Func&& func) {
    try {
        func();
        return true;
    } catch (std::exception e) {
        std::cout << e.what() << '\n';
        return false;
    }
}

inline World world;

// Dummy components
struct pos {
    float x{0.0f}, y{0.0f};
};
struct hp {
    size_t value{0};
};
struct xp {
    size_t value{0};
};

inline bool function_called = false;
inline bool lambda_called = false;

// Dummy systems
inline void function_dummy(Query<Include<pos, hp>>& q) { function_called = true; }

inline auto lambda_dummy = [](Query<Include<pos, hp>>& q) {
    lambda_called = true;
};

// Including systems
inline void function_singe_include(Query<Include<pos, hp>>& q) {
    for (auto [e, pos, hp] : q) {
    }
}

inline void function_double_include(Query<Include<pos>, Include<hp>>& q) {
    for (auto [e, pos, hp] : q) {
    }
}

inline auto lambda_singe_include = [](Query<Include<pos, hp>>& q) {
    for (auto [e, pos, hp] : q) {
    }
};

inline auto lambda_double_include = [](Query<Include<pos>, Include<hp>>& q) {
    for (auto [e, pos, hp] : q) {
    }
};

inline void function_single_exclude_0(Query<Include<xp>, Exclude<pos, hp>>& q) {}

inline void function_single_exclude_1(Query<Exclude<pos, hp>, Include<xp>>& q) {}

inline void function_double_exclude_0(Query<Include<xp>, Exclude<pos>, Exclude<hp>>& q) {}

inline void function_double_exclude_1(Query<Exclude<pos>, Include<xp>, Exclude<hp>>& q) {}

inline void function_double_exclude_2(Query<Exclude<pos>, Exclude<hp>, Include<xp>>& q) {}

inline auto lambda_single_exclude_0 = [](Query<Include<xp>, Exclude<pos, hp>>& q) {
};

inline auto lambda_single_exclude_1 = [](Query<Exclude<pos, hp>, Include<xp>>& q) {
};

inline auto lambda_double_exclude_0 = [](Query<Include<xp>, Exclude<pos>, Exclude<hp>>& q) {
};

inline auto lambda_double_exclude_1 = [](Query<Exclude<pos>, Include<xp>, Exclude<hp>>& q) {
};

inline auto lambda_double_exclude_2 = [](Query<Exclude<pos>, Exclude<hp>, Include<xp>>& q) {
};

inline void create_entity() {
    world.create_entity(pos{0.0f, 0.0f}, hp{100});

    if (world.archetype_registry.entity_records.size() < 1
        || world.archetype_registry.entity_records[0].archetype == nullptr
        || world.archetype_registry.entity_records[0].generation != 0) {
        throw std::exception("Failed to create entity");
    }
}

inline void destroy_entity() {
    Entity e{0, 0};
    world.destroy_entity(e);

    if (world.archetype_registry.entity_records[e.id].archetype != nullptr
        || world.archetype_registry.entity_records[e.id].generation != 1) {
        throw std::exception("Failed to destroy entity");
    }
}

inline void reuse_entity_id() {
    Entity e = world.create_entity(hp{200}, pos{7.0f, 8.0f});

    if (e.id != 0 || e.generation != 1) {
        throw std::exception("Failed to reuse entity id");
    }

    if (world.archetype_registry.entity_records[e.id].archetype == nullptr) {
        throw std::exception("Failed to create entity with reused id");
    }
}

inline void create_entities() {
    world.create_entities<pos, hp>(2);
    world.create_entities<hp, xp>(2, [](size_t i, auto emit) {
        emit(hp{50 * i}, xp{size_t((1.0f / float(i * i + 1)) * 100)});
    });
    world.create_entities<hp, pos>(4, [](size_t i, auto emit) {
        emit(hp{100}, pos{float(i), float(2 * i)});
    });
    world.create_entities<pos, hp, xp>(3, [](size_t i, auto emit) {
        emit(pos{float(i), float(2 * i)}, hp{50 * i}, xp{100});
    });

    if (world.archetype_registry.entity_records.size() < 12) {
        throw std::exception("Failed to create entities");
    }

    for (size_t i = 1; i < 12; i++) {
        if (world.archetype_registry.entity_records[i].archetype == nullptr
            || world.archetype_registry.entity_records[i].generation != 0) {
            throw std::exception("Failed to create entities");
        }
    }
}

inline void destroy_entities() {
    std::vector<Entity> e = {Entity{1, 0}, Entity{2, 0}};
    world.destroy_entities(e);

    for (size_t i = 1; i < 3; i++) {
        if (world.archetype_registry.entity_records[i].archetype != nullptr
            || world.archetype_registry.entity_records[i].generation != 1) {
            throw std::exception("Failed to destroy entity");
        }
    }
}

inline void system_called() {
    int id_0 = world.register_system(function_dummy);
    int id_1 = world.register_system(lambda_dummy);

    if (id_0 < 0 || id_1 < 0) {
        throw std::exception("Failed to register system(s)");
    }

    world.run();

    if (!function_called || !lambda_called) {
        throw std::exception("Failed to call system(s)");
    }

    world.unregister_system(id_0);
    world.unregister_system(id_1);
}

inline void system_query() {
    int id_0 = world.register_system(function_singe_include);
    int id_1 = world.register_system(function_double_include);
    int id_2 = world.register_system(lambda_singe_include);
    int id_3 = world.register_system(lambda_double_include);

    if (id_0 < 0 || id_1 < 0 || id_2 < 0 || id_3 < 0) {
        throw std::exception("Failed to register system(s)");
    }

    world.run();

    world.unregister_system(id_0);
    world.unregister_system(id_1);
    world.unregister_system(id_2);
    world.unregister_system(id_3);
}

inline void system_excluded_query() {
    int id_0 = world.register_system(function_single_exclude_0);
    int id_1 = world.register_system(function_single_exclude_1);
    int id_2 = world.register_system(function_double_exclude_0);
    int id_3 = world.register_system(function_double_exclude_1);
    int id_4 = world.register_system(function_double_exclude_2);
    int id_5 = world.register_system(lambda_single_exclude_0);
    int id_6 = world.register_system(lambda_single_exclude_1);
    int id_7 = world.register_system(lambda_double_exclude_0);
    int id_8 = world.register_system(lambda_double_exclude_1);
    int id_9 = world.register_system(lambda_double_exclude_2);

    if (id_0 < 0 || id_1 < 0 || id_2 < 0 || id_3 < 0 || id_4 < 0 || id_5 < 0 || id_6 < 0 || id_7 < 0
        || id_8 < 0 || id_9 < 0) {
        throw std::exception("Failed to register system(s)");
    }

    world.run();

    world.unregister_system(id_0);
    world.unregister_system(id_1);
    world.unregister_system(id_2);
    world.unregister_system(id_3);
    world.unregister_system(id_4);
    world.unregister_system(id_5);
    world.unregister_system(id_6);
    world.unregister_system(id_7);
    world.unregister_system(id_8);
    world.unregister_system(id_9);
}

inline void update_cycle();

inline void destroy_reuse_cycle();