#pragma once
#include "meta/type_list.hpp"

#include "ecs/archetype.hpp"

namespace rw_ecs::meta {

template <typename Tuple>
struct archetype_from_tuple;

template <typename... Components>
struct archetype_from_tuple<std::tuple<Components...>> {
    using type = Archetype<Components...>;
};

template <typename Lisr>
struct archetype_from_type_list;

template <typename... Components>
struct archetype_from_type_list<rw_meta::type_list<Components...>> {
    using type = Archetype<Components...>;
};

template <typename... Components>
using canonical_archetype_t =
    typename archetype_from_type_list<rw_meta::canonical_type_list_t<rw_meta::type_list<Components...>>>::type;

template <typename Tuple>
using archetype_from_tuple_t = typename archetype_from_tuple<Tuple>::type;

template <typename List>
using archetype_from_type_list_t = typename archetype_from_type_list<List>::type;

} // namespace rw_ecs::meta