#pragma once
#include <tuple>

namespace rw_ecs::meta {

template <typename... Components>
struct Include {
    using types = std::tuple<Components...>;
};

template <typename... Components>
struct Exclude {
    using types = std::tuple<Components...>;
};

template <typename T>
struct is_included : std::false_type {};
template <typename... Components>
struct is_included<Include<Components...>> : std::true_type {};

template <typename T>
struct is_excluded : std::false_type {};
template <typename... Components>
struct is_excluded<Exclude<Components...>> : std::true_type {};

template <typename T>
struct is_filter : std::false_type {};
template <typename... Components>
struct is_filter<Include<Components...>> : std::true_type {};
template <typename... Components>
struct is_filter<Exclude<Components...>> : std::true_type {};

template <typename Tuple, template <typename...> typename Func>
struct apply_components;

template <typename... Components, template <typename...> typename Func>
struct apply_components<std::tuple<Components...>, Func> {
    static void call() { Func<Components...>::apply(); }
};

template <typename... Filters>
struct get_included_components {
    using type = decltype(std::tuple_cat(
        std::conditional_t<is_included<Filters>::value, typename Filters::types, std::tuple<>>{}...
    ));
};

template <typename... Filters>
struct get_excluded_components {
    using type = decltype(std::tuple_cat(
        std::conditional_t<is_excluded<Filters>::value, typename Filters::types, std::tuple<>>{}...
    ));
};

template <typename... Filters>
struct get_components {
    using type = decltype(std::tuple_cat(
        std::conditional_t<is_filter<Filters>::value, typename Filters::types, std::tuple<>>{}...
    ));
};

} // namespace rw_ecs::meta

namespace rw_ecs {

using meta::Exclude;
using meta::Include;

} // namespace rw_ecs