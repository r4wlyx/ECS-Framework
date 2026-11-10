#pragma once
#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <vector>

#include "meta/type_list.hpp"

#include "component.hpp"
#include "entity.hpp"

namespace rw_ecs {

class IArchetype;

using VisitorFunction = std::function<void(IArchetype&, const std::function<void(size_t, void*)>&)>;

struct ArchetypeDescriptor {
    std::vector<ComponentMover> components;
    VisitorFunction visitor;
    ComponentMask mask;
    IArchetype* archetype{};
};

// whoah akiyama mizuki reference
struct ArchetypeTransition {
    struct MoveOperation {
        void* src_vec{};
        void* dst_vec{};
        MoveFunction move;
    };

    IArchetype* to{};
    std::vector<MoveOperation> moves;

    void* component_storage{};
};

struct ArchetypeRegistry {
    std::vector<ArchetypeDescriptor*> descriptors;
    std::vector<std::unique_ptr<IArchetype>> archetypes;
    std::vector<EntityRecord> entity_records;
};

class IArchetype {
  public:
    explicit IArchetype(ArchetypeDescriptor* d, ArchetypeRegistry& r)
        : descriptor(d), registry(r) {}
    virtual ~IArchetype() = default;

    ArchetypeRegistry& registry;
    ArchetypeDescriptor* descriptor;

    std::vector<Entity> entities;
    std::vector<size_t> entity_to_storage_index;

    std::vector<ArchetypeTransition> component_add_transition;
    std::vector<ArchetypeTransition> component_remove_transition;

    void visit_components(const std::function<void(size_t, void*)>& callback) {
        descriptor->visitor(*this, callback);
    }

    template <typename Component>
    IArchetype* add_component(Entity e, Component c) {
        const size_t cid = component_id<Component>();
        if (descriptor->mask.has(cid)) return this;

        ArchetypeTransition& transition = ensure_add_transition(cid);

        IArchetype* dst = transition.to;
        const size_t src_idx = entity_to_storage_index[e.id];
        const size_t dst_idx = dst->entities.size();

        dst->entities.push_back(e);

        if (e.id >= dst->entity_to_storage_index.size())
            dst->entity_to_storage_index.resize(e.id + 1, INVALID_INDEX);
        dst->entity_to_storage_index[e.id] = dst_idx;

        dst->resize_components(dst->entities.size());

        for (auto& m : transition.moves) m.move(m.src_vec, src_idx, m.dst_vec, dst_idx);

        add_component_value<Component>(transition, c, dst_idx);
        remove_entity(e);

        registry.entity_records[e.id].archetype = dst;

        return dst;
    }

    template <typename Component>
    IArchetype* batch_add_component(const std::vector<Entity>& entities, Component c) {
        if (entities.empty()) return this;

        const size_t cid = component_id<Component>();
        if (descriptor->mask.has(cid)) return this;

        ArchetypeTransition& transition = ensure_add_transition(cid);

        IArchetype* dst = transition.to;
        const size_t n_entities = entities.size();
        const size_t start_idx = dst->entities.size();

        dst->entities.resize(start_idx + n_entities);

        std::vector<size_t> src_indices = move_entities(dst, entities, start_idx);

        dst->resize_components(dst->entities.size());
        move_components(transition, src_indices, start_idx);

        for (const Entity& e : entities) {
            add_component_value<Component>(transition, c, dst->entity_to_storage_index[e.id]);
            remove_entity(e);

            registry.entity_records[e.id].archetype = dst;
        }

        return dst;
    }

    template <typename Component>
    IArchetype* batch_add_component(
        const std::vector<Entity>& entities, const std::vector<Component>& components
    ) {
        if (entities.empty()) return this;
        assert(
            components.size() == entities.size() && "Component vector size must match entities size"
        );

        const size_t cid = component_id<Component>();
        if (descriptor->mask.has(cid)) return this;

        ArchetypeTransition& transition = ensure_add_transition(cid);

        IArchetype* dst = transition.to;
        const size_t n_entities = entities.size();
        const size_t start_idx = dst->entities.size();

        dst->entities.resize(start_idx + n_entities);

        std::vector<size_t> src_indices = move_entities(dst, entities, start_idx);

        dst->resize_components(dst->entities.size());
        move_components(transition, src_indices, start_idx);

        for (size_t i = 0; i < n_entities; i++) {
            const Entity& e = entities[i];
            const Component& c = components[i];

            add_component_value<Component>(transition, c, dst->entity_to_storage_index[e.id]);
            remove_entity(e);

            registry.entity_records[e.id].archetype = dst;
        }

        return dst;
    }

    IArchetype* remove_component(Entity e, size_t cid) {
        if (!descriptor->mask.has(cid)) return this;

        ArchetypeTransition& transition = ensure_remove_transition(cid);

        IArchetype* dst = transition.to;
        const size_t src_idx = entity_to_storage_index[e.id];
        const size_t dst_idx = dst->entities.size();

        dst->entities.push_back(e);
        if (e.id >= dst->entity_to_storage_index.size())
            dst->entity_to_storage_index.resize(e.id + 1, INVALID_INDEX);
        dst->entity_to_storage_index[e.id] = dst_idx;
        dst->resize_components(dst->entities.size());

        for (auto& m : transition.moves) m.move(m.src_vec, src_idx, m.dst_vec, dst_idx);

        remove_entity(e);

        registry.entity_records[e.id].archetype = dst;

        return dst;
    }

    IArchetype* batch_remove_component(const std::vector<Entity>& entities, size_t cid) {
        if (entities.empty()) return this;

        ArchetypeTransition& transition = ensure_remove_transition(cid);

        IArchetype* dst = transition.to;
        const size_t n_entities = entities.size();
        const size_t start_idx = dst->entities.size();

        dst->entities.resize(start_idx + n_entities);

        std::vector<size_t> src_indices = move_entities(dst, entities, start_idx);

        dst->resize_components(dst->entities.size());
        move_components(transition, src_indices, start_idx);

        for (size_t i = 0; i < n_entities; i++) {
            const Entity& e = entities[i];
            remove_entity(e);

            registry.entity_records[e.id].archetype = dst;
        }

        return dst;
    }

    void remove_entity(Entity e) {
        if (e.id >= entity_to_storage_index.size()) return;

        size_t idx = entity_to_storage_index[e.id];
        if (idx == INVALID_INDEX) return;

        remove_entity_at(idx);
    }

    // Gotta refactor use vector.erase() btw it's faster
    void remove_entities(const std::vector<Entity>& to_remove) {
        if (to_remove.empty()) return;

        std::vector<size_t> indices;
        indices.reserve(to_remove.size());

        for (const Entity& e : to_remove) {
            if (e.id < entity_to_storage_index.size()) {
                size_t idx = entity_to_storage_index[e.id];
                if (idx != INVALID_INDEX) indices.push_back(idx);
            }
        }

        if (indices.empty()) return;

        std::sort(indices.begin(), indices.end(), std::greater<>());

        for (size_t idx : indices) remove_entity_at(idx);
    }

    virtual void resize_components(size_t) = 0;

  protected:
    static constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();
    virtual void remove_components_at(size_t idx) = 0;

  private:
    // gotta make sure akiyama mizuki exists
    void ensure_transition_capacity(size_t cid) {
        assert(
            component_add_transition.size() == component_remove_transition.size()
            && "Transition mirroring failed somewhere!"
        );

        if (component_add_transition.size() <= cid) {
            component_add_transition.resize(cid + 1);
            component_remove_transition.resize(cid + 1);
        }
    }

    ArchetypeDescriptor* find_descriptor_with_added(size_t added_cid) const {
        for (auto* d : registry.descriptors) {
            if (d == descriptor) continue;
            if (!d->mask.has(added_cid)) continue;
            if (!d->mask.is_superset_of(descriptor->mask)) continue;
            if (d->mask.popcount() != descriptor->mask.popcount() + 1) continue;
            return d;
        }
        return nullptr;
    }

    ArchetypeDescriptor* find_descriptor_with_removed(size_t removed_cid) const {
        for (auto* d : registry.descriptors) {
            if (d == descriptor) continue;
            if (d->mask.has(removed_cid)) continue;
            if (!descriptor->mask.is_superset_of(d->mask)) continue;
            if (descriptor->mask.popcount() != d->mask.popcount() + 1) continue;
            return d;
        }
        return nullptr;
    }

    void
    create_transition(IArchetype* destination, ArchetypeTransition& transition, size_t added_cid) {
        transition.to = destination;
        transition.moves.clear();

        visit_components([&](size_t id, void* src_vec) {
            destination->visit_components([&](size_t id2, void* dst_vec) {
                if (id != id2) return;

                for (auto& cd : descriptor->components) {
                    if (cd.id == id) {
                        transition.moves.push_back({src_vec, dst_vec, cd.move});
                        break;
                    }
                }
            });
        });

        destination->visit_components([&](size_t id, void* dst_vec) {
            if (id == added_cid) transition.component_storage = dst_vec;
        });
    }

    ArchetypeTransition& ensure_add_transition(size_t cid) {
        ensure_transition_capacity(cid);
        ArchetypeTransition& transition = component_add_transition[cid];
        if (!transition.to) {
            ArchetypeDescriptor* target_desc = find_descriptor_with_added(cid);
            assert(target_desc != nullptr && "Archetype not found!");
            create_transition(target_desc->archetype, transition, cid);
        }
        return transition;
    }

    ArchetypeTransition& ensure_remove_transition(size_t cid) {
        ensure_transition_capacity(cid);
        ArchetypeTransition& transition = component_remove_transition[cid];
        if (!transition.to) {
            ArchetypeDescriptor* target_desc = find_descriptor_with_removed(cid);
            assert(target_desc != nullptr && "Archetype not found!");
            create_transition(target_desc->archetype, transition, cid);
        }
        return transition;
    }

    std::vector<size_t>
    move_entities(IArchetype* dst, const std::vector<Entity>& entities, size_t start_idx) {
        const size_t n = entities.size();

        std::vector<size_t> src_indices(n);

        size_t max_id = 0;
        for (const Entity& e : entities)
            if (e.id > max_id) max_id = e.id;

        if (dst->entity_to_storage_index.size() <= max_id)
            dst->entity_to_storage_index.resize(max_id + 1, INVALID_INDEX);

        for (size_t i = 0; i < n; i++) {
            const Entity& e = entities[i];
            src_indices[i] = entity_to_storage_index[e.id];
            dst->entities[start_idx + i] = e;
            dst->entity_to_storage_index[e.id] = start_idx + i;
        }

        return src_indices;
    }

    void move_components(
        ArchetypeTransition& transition, const std::vector<size_t>& src_indices, size_t start_idx
    ) {
        const size_t n = src_indices.size();

        for (auto& m : transition.moves) {
            void* src = m.src_vec;
            void* dst = m.dst_vec;

            for (size_t i = 0; i < n; i++) m.move(src, src_indices[i], dst, start_idx + i);
        }
    }

    template <typename Component>
    void add_component_value(ArchetypeTransition& transition, const Component& c, size_t index) {
        using Vec = std::vector<std::remove_reference_t<Component>>;
        (*static_cast<Vec*>(transition.component_storage))[index] = c;
    }

    void remove_entity_at(size_t idx) {
        const size_t last = entities.size() - 1;

        Entity removed = entities[idx];

        if (idx != last) {
            std::swap(entities[idx], entities[last]);
            entity_to_storage_index[entities[idx].id] = idx;
        }

        entities.pop_back();
        entity_to_storage_index[removed.id] = INVALID_INDEX;

        remove_components_at(idx);
    }
};

template <typename... Components>
class Archetype : public IArchetype {
  public:
    using ComponentTypes = typename rw_meta::type_list<Components...>;
    std::tuple<std::vector<Components>...> components_storage;

    Archetype(ArchetypeRegistry& registry) : IArchetype(get_descriptor(registry), registry) {
        assert(descriptor->archetype == nullptr && "Duplicate Archetype instance!");
        descriptor->archetype = this;
    }

    static ArchetypeDescriptor* get_descriptor(ArchetypeRegistry& registry) {
        static ArchetypeDescriptor desc{
            [] {
                std::vector<ComponentMover> moves{ComponentMover{
                    component_id<Components>(), [](void* src, size_t si, void* dst, size_t di) {
                        using Vec = std::vector<Components>;
                        auto& s = (*static_cast<Vec*>(src))[si];
                        (*static_cast<Vec*>(dst))[di] = std::move(s);
                    }
                }...};
                std::sort(moves.begin(), moves.end(), [](auto& a, auto& b) { return a.id < b.id; });
                return moves;
            }(),
            [](IArchetype& base, const std::function<void(size_t, void*)>& f) {
                auto& self = static_cast<Archetype<Components...>&>(base);
                (f(component_id<Components>(),
                   &std::get<std::vector<Components>>(self.components_storage)),
                 ...);
            }
        };

        static bool registered = [&] {
            desc.mask = build_mask(desc.components);
            registry.descriptors.push_back(&desc);
            return true;
        }();
        (void)registered;

        return &desc;
    }

    void resize_components(size_t n) override {
        auto first_size = std::get<0>(components_storage).size();
        if (first_size >= n) return;
        std::apply([&](auto&... vecs) { (vecs.resize(n), ...); }, components_storage);
    }

    Entity add_entity(Entity e, Components&&... components) {
        size_t index = entities.size();

        entities.push_back(e);
        if (e.id >= entity_to_storage_index.size())
            entity_to_storage_index.resize(e.id + 1, INVALID_INDEX);
        entity_to_storage_index[e.id] = index;

        registry.entity_records[e.id].archetype = this;

        std::apply(
            [&](std::vector<Components>&... vecs) {
                (vecs.push_back(std::forward<Components>(components)), ...);
            },
            components_storage
        );

        return e;
    }

    std::vector<Entity> add_entities(const std::vector<Entity>& new_entities) {
        if (new_entities.empty()) return new_entities;

        const size_t n_entities = new_entities.size();
        const size_t start_idx = entities.size();

        entities.resize(start_idx + n_entities);

        size_t max_id = 0;
        for (const Entity& e : new_entities)
            if (e.id > max_id) max_id = e.id;

        if (entity_to_storage_index.size() <= max_id)
            entity_to_storage_index.resize(max_id + 1, INVALID_INDEX);

        for (size_t i = 0; i < n_entities; i++) {
            const Entity& e = new_entities[i];
            entities[start_idx + i] = e;
            entity_to_storage_index[e.id] = start_idx + i;

            registry.entity_records[e.id].archetype = this;
        }

        std::apply(
            [&](auto&... vecs) { (vecs.resize(vecs.size() + n_entities), ...); }, components_storage
        );
        return new_entities;
    }

    template <typename Callback>
        requires(
            std::is_invocable_r_v<void, Callback, size_t, std::function<void(Components && ...)>>
        )
    std::vector<Entity> add_entities(const std::vector<Entity>& new_entities, Callback&& callback) {
        if (new_entities.empty()) return new_entities;

        const size_t n_entities = new_entities.size();
        const size_t start_idx = entities.size();

        entities.resize(start_idx + n_entities);

        size_t max_id = 0;
        for (const Entity& e : new_entities)
            if (e.id > max_id) max_id = e.id;

        if (entity_to_storage_index.size() <= max_id)
            entity_to_storage_index.resize(max_id + 1, INVALID_INDEX);

        for (size_t i = 0; i < n_entities; i++) {
            const Entity& e = new_entities[i];
            entities[start_idx + i] = e;
            entity_to_storage_index[e.id] = start_idx + i;

            registry.entity_records[e.id].archetype = this;
        }

        std::apply(
            [&](std::vector<Components>&... vecs) {
                (vecs.reserve(vecs.size() + n_entities), ...);
                for (size_t i = 0; i < n_entities; i++)
                    callback(i, [&](Components... components) {
                        (vecs.push_back(std::move(components)), ...);
                    });
            },
            components_storage
        );
        return new_entities;
    }

  protected:
    void remove_components_at(size_t idx) override {
        (
            [&] {
                auto& vec = std::get<std::vector<Components>>(components_storage);
                std::swap(vec[idx], vec.back());
                vec.pop_back();
            }(),
            ...
        );
    }
};

} // namespace rw_ecs