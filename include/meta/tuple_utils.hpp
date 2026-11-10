#pragma once
#include <tuple>

#include "print_type.hpp"

namespace rw_meta {

template <typename T, typename Tuple>
struct first_index_of;

template <typename T, typename... Ts>
struct first_index_of<T, std::tuple<T, Ts...>> {
    static constexpr size_t value = 0;
};

template <typename T, typename U, typename... Ts>
struct first_index_of<T, std::tuple<U, Ts...>> {
    static constexpr size_t value = 1 + first_index_of<T, std::tuple<Ts...>>::value;
};

template <typename T>
struct first_index_of<T, std::tuple<>> {
    static_assert(always_false_v<T>, "Type not found in tuple");
};

template <typename T, typename Tuple>
constexpr size_t first_index_of_v = first_index_of<T, Tuple>::value;

} // namespace rw_meta