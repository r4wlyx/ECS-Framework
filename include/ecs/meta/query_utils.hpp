#pragma once
#include "ecs/query.hpp"

namespace rw_ecs::meta {

template <typename Tuple, size_t I = 0, bool End = (I >= std::tuple_size_v<Tuple>)>
struct find_query;

template <typename Tuple, size_t I>
struct find_query<Tuple, I, true> {
    using type = void;
};

template <typename Tuple, size_t I>
struct find_query<Tuple, I, false> {
  private:
    using T = std::remove_cvref_t<std::tuple_element_t<I, Tuple>>;

  public:
    using type = std::conditional_t<std::is_base_of_v<IQuery, T>, T, typename find_query<Tuple, I + 1>::type>;
};

} // namespace rw_ecs::meta