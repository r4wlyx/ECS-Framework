#pragma once

namespace rw_meta {

template <typename>
inline constexpr bool always_false_v = false;

template <typename T>
struct print_type {
    static_assert(always_false_v<T>, "print_type instantiated");
};

} // namespace rw_meta