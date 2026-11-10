#pragma once
#include <functional>

namespace rw_ecs {

struct System {
    enum class Phase { Start, Update, Render, Cleanup, Count };
    std::function<void()> func;
};

} // namespace rw_ecs