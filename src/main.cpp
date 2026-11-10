#include <iostream>

#include "test.hpp"

int main() {
    size_t tests = 6;
    size_t passed = 0;

    passed += test(create_entity);
    passed += test(destroy_entity);
    passed += test(reuse_entity_id);
    passed += test(create_entities);
    passed += test(destroy_entities);
    passed += test(system_called);
    // passed += test(system_query);
    // passed += test(system_excluded_query);

    std::cout << "Tests passed: " << passed << '/' << tests << '\n';
}

/*

int main() {
    World world;

    // 1. Create a few entities
    world.create_entity(pos{1, 2}, hp{100});
    world.create_entity(pos{3, 4}, hp{50}, pos{0, 2}); // duplicate components
    world.create_entity(pos{5, 6}, hp{75});
    world.create_entity(pos{9, 10}, hp{200}, shield{50}); // entity with extra component

    // Check entity records
    assert(world.archetype_registry.entity_records.size() >= 4);
    for (size_t i = 0; i < 4; i++) {
        assert(world.archetype_registry.entity_records[i].archetype != nullptr);
        assert(world.archetype_registry.entity_records[i].generation == 0);
    }

    // 2. Destroy one entity
    Entity e1{1, 0}; // assuming sequential IDs from new_entity
    world.destroy_entity(e1);

    // Verify entity was removed and generation incremented
    assert(world.archetype_registry.entity_records[e1.id].archetype == nullptr);
    assert(world.archetype_registry.entity_records[e1.id].generation == 1);

    // 3. Create a new entity, should reuse ID 1 with incremented generation
    Entity e4 = world.new_entity();
    assert(e4.id == 1);
    assert(e4.generation == 1);

    // 4. Add components to reused entity
    world.create_entity(pos{7, 8}, hp{200}); // fills reused entity ID 1
    assert(world.archetype_registry.entity_records[e4.id].archetype != nullptr);

    // 5. Register a system that queries pos & hp (Include)
    bool system_called = false;
    world.register_system([&](Query<Include<pos, hp>>& q) {
        system_called = true;
        size_t count = 0;
        for (auto [entity, p, h] : q) {
            assert(entity.id < world.archetype_registry.entity_records.size());
            assert(p.x >= 0 && p.y >= 0);
            assert(h.value >= 0);
            count++;
        }
        // Expect 4 entities total
        assert(count == 4);
    });

    // 6. Register a system that queries pos & hp but excludes shield
    bool exclude_called = false;
    world.register_system([&](Query<Include<pos, hp>, Exclude<shield>>& q) {
        exclude_called = true;
        size_t count = 0;
        for (auto [entity, p, h] : q) {
            assert(entity.id < world.archetype_registry.entity_records.size());
            assert(p.x >= 0 && p.y >= 0);
            assert(h.value >= 0);
            count++;
        }
        // Only entities without shield should appear (3 entities)
        assert(count == 3);
    });

    // 7. Simulate update loop
    for (System& sys : world.update_systems) sys.func();

    assert(system_called && "Include query system was not called!");
    assert(exclude_called && "Exclude query system was not called!");

    // 8. Test multiple destroy/reuse cycles
    Entity e_destroy = Entity{2, 0};
    world.destroy_entity(e_destroy);
    assert(world.archetype_registry.entity_records[e_destroy.id].generation == 1);

    Entity e_reuse = world.new_entity();
    // This just gives the next free ID (2) but does not mark it in the registry yet

    Entity e_filled = world.create_entity(pos{11, 12}, hp{300});
    // create_entity internally calls new_entity() again; this is the actual ID in the registry

    // Now we can assert that the new entity filled the same ID we expected
    assert(e_filled.id == e_reuse.id);                 // check ID matches
    assert(e_filled.generation == e_reuse.generation); // generation is correct
    assert(world.archetype_registry.entity_records[e_filled.id].archetype != nullptr);

    // 9. Multi-cycle destroy & reuse test with update systems
    bool update_system_called = false;
    world.register_system(
        [&](Query<Include<pos, hp>>& q) {
            update_system_called = true;
            // Just count entities to ensure queries work in the update loop
            size_t count = 0;
            for (auto [entity, p, h] : q) {
                assert(entity.id < world.archetype_registry.entity_records.size());
                count++;
            }
            assert(count > 0);
        },
        System::Phase::Update
    );

    // Perform several destroy/reuse cycles
    for (int cycle = 0; cycle < 5; ++cycle) {
        // Destroy entities with even IDs
        for (uint32_t id = 0; id < world.archetype_registry.entity_records.size(); id += 2) {
            Entity e{id, world.archetype_registry.entity_records[id].generation};
            if (world.archetype_registry.entity_records[id].archetype != nullptr) {
                world.destroy_entity(e);
                assert(world.archetype_registry.entity_records[id].archetype == nullptr);
            }
        }

        // Create new entities to fill freed IDs
        for (int i = 0; i < 3; ++i) {
            Entity new_e = world.new_entity();
            auto filled = world.create_entity(pos{float(i), float(i)}, hp{10 * i});
            // Check ID reuse
            assert(filled.id == new_e.id);
            assert(filled.generation == new_e.generation);
            assert(world.archetype_registry.entity_records[filled.id].archetype != nullptr);
        }

        // Run update systems
        for (System& sys : world.update_systems) sys.func();
        assert(update_system_called && "Update system was not called in cycle!");
        update_system_called = false;
    }

    std::cout << "All ECS tests passed!" << std::endl;
    return 0;
}

*/