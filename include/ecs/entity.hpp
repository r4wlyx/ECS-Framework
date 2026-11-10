#pragma once
#include <cstdint>

namespace rw_ecs {

class IArchetype;

struct Entity {
    uint32_t id;
    uint32_t generation;
};

struct EntityRecord {
    IArchetype* archetype{};
    uint32_t generation;
};

} // namespace rw_ecs