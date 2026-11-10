#pragma once
#include <tuple>
#include <type_traits>

#include "type_id.hpp"

namespace rw_meta {

template <typename... Ts>
struct type_list {};

template <typename T, typename List>
struct type_list_contains;

template <typename T, typename... Ts>
struct type_list_contains<T, type_list<Ts...>>
    : std::bool_constant<(std::is_same_v<T, Ts> || ...)> {};

template <typename List>
struct type_list_unique;

template <>
struct type_list_unique<type_list<>> : std::true_type {};

template <typename Head, typename... Tail>
struct type_list_unique<type_list<Head, Tail...>>
    : std::bool_constant<
          !type_list_contains<Head, type_list<Tail...>>::value
          && type_list_unique<type_list<Tail...>>::value> {};

template <typename List>
struct type_list_size;

template <typename... Ts>
struct type_list_size<type_list<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> {};

template <typename T, typename List>
struct type_list_index_of;

template <typename T, typename... Ts>
struct type_list_index_of<T, rw_meta::type_list<T, Ts...>> : std::integral_constant<size_t, 0> {};

template <typename T, typename U, typename... Ts>
struct type_list_index_of<T, rw_meta::type_list<U, Ts...>>
    : std::integral_constant<size_t, 1 + type_list_index_of<T, rw_meta::type_list<Ts...>>::value> {};

namespace detail {

    template <typename T, typename List>
    struct prepend;

    template <typename T, typename... Ts>
    struct prepend<T, type_list<Ts...>> {
        using type = type_list<T, Ts...>;
    };

    template <typename List, typename T>
    struct insert_sorted;

    template <typename T>
    struct insert_sorted<type_list<>, T> {
        using type = type_list<T>;
    };

    template <typename Head, typename... Tail, typename T>
    struct insert_sorted<type_list<Head, Tail...>, T> {
        using type = std::conditional_t<
            type_id_less_v<T, Head>,
            type_list<T, Head, Tail...>,
            typename prepend<Head, typename insert_sorted<type_list<Tail...>, T>::type>::type>;
    };

    template <typename List>
    struct to_tuple;

    template <typename... Ts>
    struct to_tuple<type_list<Ts...>> {
        using type = std::tuple<Ts...>;
    };

    template <typename Tuple>
    struct from_tuple;

    template <typename... Ts>
    struct from_tuple<std::tuple<Ts...>> {
        using type = type_list<Ts...>;
    };

} // namespace detail

template <typename List>
struct unique_type_list;

template <>
struct unique_type_list<type_list<>> {
    using type = type_list<>;
};

template <typename Head, typename... Tail>
struct unique_type_list<type_list<Head, Tail...>> {
  private:
    using tail = typename unique_type_list<type_list<Tail...>>::type;

  public:
    using type = std::conditional_t<
        type_list_contains<Head, tail>::value,
        tail,
        typename detail::prepend<Head, tail>::type>;
};

template <typename List>
struct sort_type_list;

template <>
struct sort_type_list<type_list<>> {
    using type = type_list<>;
};

template <typename Head, typename... Tail>
struct sort_type_list<type_list<Head, Tail...>> {
  private:
    using rest_sorted = typename sort_type_list<type_list<Tail...>>::type;

  public:
    using type = typename detail::insert_sorted<rest_sorted, Head>::type;
};

template <typename List>
struct canonical_type_list {
    using type = typename sort_type_list<typename unique_type_list<List>::type>::type;
};

template <typename T, typename List>
inline constexpr bool type_list_contains_v = type_list_contains<T, List>::value;

template <typename List>
inline constexpr bool type_list_unique_v = type_list_unique<List>::value;

template <typename List>
inline constexpr std::size_t type_list_size_v = type_list_size<List>::value;

template <typename List>
inline constexpr bool type_list_empty_v = type_list_size_v<List> == 0;

template <typename T, typename List>
inline constexpr size_t type_list_index_of_v = type_list_index_of<T, List>::value;

template <typename List>
using unique_type_list_t = typename unique_type_list<List>::type;

template <typename List>
using sort_type_list_t = typename sort_type_list<List>::type;

template <typename List>
using canonical_type_list_t = typename canonical_type_list<List>::type;

template <typename List>
using to_tuple_t = typename detail::to_tuple<List>::type;

template <typename Tuple>
using from_tuple_t = typename detail::from_tuple<Tuple>::type;

} // namespace rw_meta