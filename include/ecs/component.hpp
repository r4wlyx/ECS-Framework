#pragma once
#include <vector>

namespace rw_ecs {

using MoveFunction = void (*)(void* src, size_t si, void* dst, size_t di);

struct ComponentMover {
    size_t id;
    MoveFunction move;
};

inline size_t next_component_id() {
    static size_t id = 0;
    return id++;
}

template <typename T>
size_t component_id() {
    static size_t id = next_component_id();
    return id;
}

struct ComponentMask {
    std::vector<uint64_t> bits;

    ComponentMask() = default;
    explicit ComponentMask(size_t size) : bits(size, 0) {}

    void set(size_t id) {
        size_t idx = id / 64;
        if (idx >= bits.size()) bits.resize(idx + 1, 0);
        bits[idx] |= (1ull << (id % 64));
    }

    bool has(size_t id) const {
        size_t idx = id / 64;
        return idx < bits.size() && (bits[idx] & (1ull << (id % 64))) != 0;
    }

    size_t popcount() const {
        size_t count = 0;
        for (auto b : bits) count += __builtin_popcountll(b);
        return count;
    }

    bool intersects(const ComponentMask& other) const {
        size_t n = std::min(bits.size(), other.bits.size());
        for (size_t i = 0; i < n; ++i)
            if (bits[i] & other.bits[i]) return true;
        return false;
    }

    bool is_superset_of(const ComponentMask& other) const {
        size_t n = other.bits.size();
        for (size_t i = 0; i < n; ++i) {
            uint64_t other_bits = other.bits[i];
            uint64_t this_bits = i < bits.size() ? bits[i] : 0;
            if ((this_bits & other_bits) != other_bits) return false;
        }
        return true;
    }
};

inline ComponentMask build_mask(const std::vector<ComponentMover>& comps) {
    if (comps.empty()) return {};

    size_t max_id = 0;
    for (auto& c : comps) max_id = std::max(max_id, c.id);

    ComponentMask mask((max_id / 64) + 1);
    for (auto& c : comps) mask.bits[c.id / 64] |= (1ull << (c.id % 64));
    return mask;
}

} // namespace rw_ecs