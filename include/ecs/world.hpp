#pragma once
#include <array>
#include <chrono>
#include <memory>
#include <typeindex>
#include <unordered_map>

#include "meta/archetype_utils.hpp"
#include "meta/function_traits.hpp"
#include "meta/query_utils.hpp"
#include "meta/type_list.hpp"

#include "archetype.hpp"
#include "commands.hpp"
#include "ecs/commands.hpp"
#include "entity.hpp"
#include "query.hpp"
#include "system.hpp"

namespace rw_ecs {

class World {
  public:
    World() : commands(*this) {}

    template <typename Func>
    auto make_system(Func&& system) {
        using ArgsTuple = rw_meta::to_tuple_t<rw_meta::function_argument_types_t<Func>>;
        using QueryType = typename meta::find_query<ArgsTuple>::type;

        QueryType* query_ptr = nullptr;

        if constexpr (!std::is_same_v<QueryType, void>) {
            pre_allocate_archetype<QueryType>();

            auto [it, inserted] = queries.try_emplace(
                typeid(QueryType), std::make_unique<QueryType>(archetype_registry.archetypes)
            );

            query_ptr = static_cast<QueryType*>(it->second.get());
        }

        return [query_ptr, system = std::forward<Func>(system), this]() mutable {
            auto args = make_system_args<QueryType, ArgsTuple>(query_ptr, this->commands);
            std::apply(system, args);
        };
    }

    template <typename Func>
    int register_system(Func&& system, System::Phase phase = System::Phase::Start) {
        auto sys_lambda = make_system(std::forward<Func>(system));
        size_t id = new_system();

        auto& vec = systems[static_cast<size_t>(phase)];
        vec.push_back(System{sys_lambda});
        size_t idx = vec.size() - 1;

        if (id >= system_id_to_index.size()) {
            size_t new_size = std::max(system_id_to_index.size() * 2, size_t(id + 1024));
            system_id_to_index.resize(new_size);
        }
        system_id_to_index[id] = idx;

        if (idx >= system_index_to_id.size()) {
            size_t new_size = std::max(system_index_to_id.size() * 2, size_t(idx + 1024));
            system_index_to_id.resize(new_size);
        }
        system_index_to_id[idx] = id;

        return id;
    }

    void unregister_system(int system_id, System::Phase phase) {
        size_t idx = system_id_to_index[system_id];
        auto& vec = systems[static_cast<size_t>(phase)];

        const size_t last = vec.size() - 1;

        if (idx != last) {
            std::swap(vec[idx], vec[last]);
            
            size_t moved_id = system_index_to_id[last];

            system_id_to_index[moved_id] = idx;
            system_index_to_id[idx] = moved_id;
        }
        
        vec.pop_back();

        free_system_ids.push_back(id);
    }

    template <typename... Components>
    Entity create_entity(Components&&... components) {
        using InputList = rw_meta::type_list<Components...>;

        static_assert(
            rw_meta::type_list_unique_v<InputList>, "Duplicate component types are not allowed"
        );

        using Arch = meta::canonical_archetype_t<std::decay_t<Components>...>;
        Arch* arch = get_or_create_archetype<Arch>();

        return add_entity_from_tuple<Arch>(
            arch, std::forward_as_tuple(std::forward<Components>(components)...)
        );
    }

    template <typename... Components>
    std::vector<Entity> create_entities(size_t n) {
        using InputList = rw_meta::type_list<Components...>;

        static_assert(
            rw_meta::type_list_unique_v<InputList>, "Duplicate component types are not allowed"
        );

        using Arch = meta::canonical_archetype_t<std::decay_t<Components>...>;
        Arch* arch = get_or_create_archetype<Arch>();

        std::vector<Entity> entities(n);
        for (size_t i = 0; i < n; i++) entities[i] = new_entity();

        return arch->add_entities(entities);
    }

    template <typename... Components, typename Callback>
    std::vector<Entity> create_entities(size_t n, Callback&& callback) {
        using InputList = rw_meta::type_list<Components...>;

        static_assert(
            rw_meta::type_list_unique_v<InputList>, "Duplicate component types are not allowed"
        );

        using Arch = meta::canonical_archetype_t<std::decay_t<Components>...>;
        Arch* arch = get_or_create_archetype<Arch>();

        std::vector<Entity> entities(n);
        for (size_t i = 0; i < n; i++) entities[i] = new_entity();

        auto wrapped = make_reordered_callback<Arch, Callback, Components...>(
            std::forward<Callback>(callback)
        );

        return arch->add_entities(entities, std::move(wrapped));
    }

    void destroy_entity(Entity e) {
        uint32_t id = e.id;
        if (id >= archetype_registry.entity_records.size()) return;
        IArchetype* arch = archetype_registry.entity_records[id].archetype;
        if (arch) {
            arch->remove_entity(e);
            archetype_registry.entity_records[id].archetype = nullptr;
        }
        free_entity_ids.push_back(id);
        archetype_registry.entity_records[id].generation++;
    }

    // Gotta make better
    void destroy_entities(const std::vector<Entity>& entities) {
        for (const Entity& e : entities) {
            uint32_t id = e.id;
            if (id >= archetype_registry.entity_records.size()) return;
            IArchetype* arch = archetype_registry.entity_records[id].archetype;
            if (arch) {
                arch->remove_entity(e);
                archetype_registry.entity_records[id].archetype = nullptr;
            }
            free_entity_ids.push_back(id);
            archetype_registry.entity_records[id].generation++;
        }
    }

    void run() {
        using clock = std::chrono::high_resolution_clock;

        // will change so it can be set at runtime
        constexpr float fixed_dt = 1.0f / 60.0f;
        float accumulator = 0.0f;

        running = systems[(size_t)System::Phase::Update].size() > 0;
        refresh_queries();

        for (System& system : systems[(size_t)System::Phase::Start]) system.func();

        auto prev = clock::now();

        while (running) {
            auto now = clock::now();
            float frame_dt = std::chrono::duration<float>(now - prev).count();
            frame_dt = std::min(frame_dt, fixed_dt);
            prev = now;

            accumulator += frame_dt;

            while (accumulator >= fixed_dt) {
                for (System& system : systems[(size_t)System::Phase::Update]) system.func();
                accumulator -= fixed_dt;
            }

            for (System& system : systems[(size_t)System::Phase::Render]) system.func();
        }

        for (System& system : systems[(size_t)System::Phase::Cleanup]) system.func();
    }

    //   private:
    using QueryRegistry = typename std::unordered_map<std::type_index, std::unique_ptr<IQuery>>;

    ArchetypeRegistry archetype_registry;
    QueryRegistry queries;

    friend struct Commands;

    Commands commands;
    std::vector<std::function<void(World&)>> actions;

    std::array<std::vector<System>, 4> systems;
    std::vector<size_t> system_id_to_index;
    std::vector<size_t> system_index_to_id;

    bool running = false;

    std::vector<size_t> free_entity_ids;
    size_t next_entity_id = 0;

    std::vector<size_t> free_system_ids;
    size_t next_system_id = 0;

    template <typename Pack, size_t... Is>
    void pre_allocate_archetype_impl(std::index_sequence<Is...>) {
        using Arch = meta::canonical_archetype_t<std::tuple_element_t<Is, Pack>...>;
        auto arch = std::make_unique<Arch>(archetype_registry);
        archetype_registry.archetypes.push_back(std::move(arch));
    }

    template <typename QueryType>
    void pre_allocate_archetype() {
        using IncludedComponents = typename QueryType::IncludedComponents;
        pre_allocate_archetype_impl<IncludedComponents>(
            std::make_index_sequence<std::tuple_size_v<IncludedComponents>>{}
        );
    }

    void refresh_queries() {
        for (auto& query : queries) {
            query.second->refresh_chunks();
        }
    }

    template <typename T, typename QueryType>
    decltype(auto) resolve_arg(QueryType* query_ptr, Commands& commands) {
        using Clean = std::remove_cvref_t<T>;

        if constexpr (std::is_base_of_v<IQuery, Clean>) {
            return *query_ptr;
        } else if constexpr (std::is_same_v<Clean, Commands>) {
            return commands;
        } else {
            static_assert(!sizeof(T), "Unsupported system argument type");
        }
    }

    template <typename QueryType, typename Tuple, size_t... I>
    auto
    make_system_args_impl(QueryType* query_ptr, Commands& commands, std::index_sequence<I...>) {
        return std::tuple{resolve_arg<std::tuple_element_t<I, Tuple>>(query_ptr, commands)...};
    }

    template <typename QueryType, typename Tuple>
    auto make_system_args(QueryType* query_ptr, Commands& commands) {
        return make_system_args_impl<QueryType, Tuple>(
            query_ptr, commands, std::make_index_sequence<std::tuple_size_v<Tuple>>{}
        );
    }

    template <typename Arch>
    Arch* get_or_create_archetype() {
        ArchetypeDescriptor* desc = Arch::get_descriptor(archetype_registry);

        if (!desc->archetype) {
            auto new_arch = std::make_unique<Arch>(archetype_registry);
            desc->archetype = new_arch.get();
            archetype_registry.archetypes.push_back(std::move(new_arch));

            refresh_queries();
        }

        return static_cast<Arch*>(desc->archetype);
    }

    Entity new_entity() {
        uint32_t id{0};
        uint32_t gen{0};

        if (!free_entity_ids.empty()) {
            id = free_entity_ids.back();
            free_entity_ids.pop_back();
            gen = archetype_registry.entity_records[id].generation;
        } else {
            id = next_entity_id++;
            size_t size = archetype_registry.entity_records.size();
            if (id >= size) {
                size_t new_size = std::max(size * 2, size_t(id + 1024));
                archetype_registry.entity_records.resize(new_size);
            }
            archetype_registry.entity_records[id].generation = 0;
        }

        return Entity{id, gen};
    }

    size_t new_system() {
        uint32_t id{0};

        if (!free_system_ids.empty()) {
            id = free_system_ids.back();
            free_system_ids.pop_back();
        } else {
            id = next_system_id++;
        }

        return id;
    }

    template <typename Arch, typename Tuple, size_t... Is>
    Entity add_entity_from_tuple_impl(Arch* arch, Tuple&& tuple, std::index_sequence<Is...>) {
        using ComponentTuple = rw_meta::to_tuple_t<typename Arch::ComponentTypes>;
        using ArgTuple = std::remove_reference_t<Tuple>;

        return arch->add_entity(
            new_entity(),
            std::forward<std::tuple_element_t<Is, ComponentTuple>>(
                std::get<std::tuple_element_t<Is, ComponentTuple>&&>(std::forward<Tuple>(tuple))
            )...
        );
    }

    template <typename Arch, typename Tuple>
    Entity add_entity_from_tuple(Arch* arch, Tuple&& tuple) {
        using ComponentTypes = typename Arch::ComponentTypes;
        constexpr size_t N = rw_meta::type_list_size_v<ComponentTypes>;
        return add_entity_from_tuple_impl(
            arch, std::forward<Tuple>(tuple), std::make_index_sequence<N>{}
        );
    }

    template <typename ArchList, typename InputList, typename Tuple, size_t... Is>
    auto reorder_tuple_impl(Tuple&& tup, std::index_sequence<Is...>) {
        return std::forward_as_tuple(
            std::get<rw_meta::type_list_index_of_v<std::tuple_element_t<Is, ArchList>, InputList>>(
                std::forward<Tuple>(tup)
            )...
        );
    }

    template <typename Arch, typename Callback, typename... Components>
    auto make_reordered_callback(Callback&& cb) {
        using ArchList = rw_meta::to_tuple_t<typename Arch::ComponentTypes>;
        using InputList = rw_meta::type_list<Components...>;

        constexpr size_t N = std::tuple_size_v<ArchList>;

        return [cb = std::forward<Callback>(cb), this](size_t i, auto emit_internal) mutable {
            auto emit_reordered = [&](auto&&... args) {
                auto tup = std::forward_as_tuple(std::forward<decltype(args)>(args)...);

                auto reordered = reorder_tuple_impl<ArchList, InputList>(
                    std::move(tup), std::make_index_sequence<N>{}
                );

                std::apply(emit_internal, std::move(reordered));
            };

            cb(i, emit_reordered);
        };
    }
};

template <typename... Components>
void Commands::create_entity(Components&&... components) {
    world.actions.emplace_back([tuple = std::tuple<std::decay_t<Components>...>(
                                    std::forward<Components>(components)...
                                )](World& world) {
        std::apply(
            [&](auto&&... c) { world.create_entity(std::forward<decltype(c)>(c)...); },
            std::move(tuple)
        );
    });
}

template <typename... Components>
void Commands::create_entities(size_t n) {
    world.actions.emplace_back([n](World& world) { world.create_entities<Components...>(n); });
}

template <typename... Components, typename Callback>
void Commands::create_entities(size_t n, Callback&& callback) {
    world.actions.emplace_back([n, cb = std::forward<Callback>(callback)](World& world) mutable {
        world.create_entities<Components...>(n, std::move(cb));
    });
}

// change so there's only one action for destroying entities and just store a to_be_destroyed vector
inline void Commands::destroy_entity(Entity e) {
    world.actions.emplace_back([e](World& world) { world.destroy_entity(e); });
}

inline void Commands::destroy_entities(const std::vector<Entity>& entities) {
    world.actions.emplace_back([&e = entities](World& world) { world.destroy_entities(e); });
}

// template <typename C>
// void Commands::add_component(Entity& entity) {
//     world.actions.emplace_back([e = &entity](World& world) {
//         if (!e->archetype) return;
//         e->archetype->add_component<C>(*e, C{});
//     });
// }

// template <typename C>
// void Commands::add_component(Entity& entity, C component) {
//     world.actions.emplace_back([e = &entity, component =
//     std::move(component)](World& world) mutable {
//         if (!e->archetype) return;
//         e->archetype->add_component<C>(*e, std::move(component));
//     });
// }

// template <typename C>
// void Commands::add_component(const std::vector<Entity*>& entities) {
//     world.actions.emplace_back([entities](World& world) mutable {
//         std::unordered_map<IArchetype*, std::vector<Entity*>> by_arch;
//         for (Entity* e : entities)
//             if (e->archetype) by_arch[e->archetype].push_back(e);

//         for (auto& [arch, ents] : by_arch) arch->batch_add_component(ents,
//         C{});
//     });
// }

// template <typename C>
// void Commands::add_component(const std::vector<Entity*>& entities, C
// component) {
//     world.actions.emplace_back([entities, component =
//     std::move(component)](World& world) mutable {
//         // std::vector<Entity*> valid;
//         // valid.reserve(entities.size());

//         // for (Entity* e : entities)
//         //     if (e->archetype) valid.push_back(e);
//         // if (valid.empty()) return;

//         // valid[0]->archetype->batch_add_component(valid,
//         std::move(component));

//         for (Entity* e : entities)
//             if (e->archetype) e->archetype->add_component(*e, component);
//     });
// }

} // namespace rw_ecs