#pragma once
#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "meta/query_filters.hpp"

#include "archetype.hpp"
#include "component.hpp"
#include "entity.hpp"

namespace rw_ecs {

template <typename Component>
struct ComponentChunk {
    IArchetype* archetype{};
    std::vector<Component>* data{};
    std::vector<Entity>* entities{};
};

template <typename Component>
struct ComponentQuery {
    std::vector<ComponentChunk<Component>> chunks;

    struct iterator;

    iterator begin() const {
        size_t o = 0, i = 0;
        while (o < chunks.size() && chunks[o].data->empty()) ++o;
        return {&chunks, o, i};
    }

    iterator end() const { return {&chunks, chunks.size(), 0}; }
};

template <typename Component>
class ComponentQuery<Component>::iterator {
    const std::vector<ComponentChunk<Component>>* chunks_ptr = nullptr;
    size_t outer_idx = 0;
    size_t inner_idx = 0;

    iterator() = default;
    iterator(const std::vector<ComponentChunk<Component>>* c, size_t o, size_t i)
        : chunks_ptr(c), outer_idx(o), inner_idx(i) {}

    Component& operator*() { return (*chunks_ptr)[outer_idx].data->at(inner_idx); }

    Component* operator->() { return &(*chunks_ptr)[outer_idx].data->at(inner_idx); }

    iterator& operator++() {
        ++inner_idx;
        while (outer_idx < chunks_ptr->size() && inner_idx >= (*chunks_ptr)[outer_idx].data->size()) {
            ++outer_idx;
            inner_idx = 0;
        }
        return *this;
    }

    iterator operator++(int) {
        iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    friend bool operator==(const iterator& a, const iterator& b) {
        return a.chunks_ptr == b.chunks_ptr && a.outer_idx == b.outer_idx && a.inner_idx == b.inner_idx;
    }

    friend bool operator!=(const iterator& a, const iterator& b) { return !(a == b); }
};

template <typename... Filters>
concept ValidQueryFilters = (meta::is_filter<Filters>::value && ...)
                            && (std::tuple_size_v<typename meta::get_included_components<Filters...>::type> > 0);

struct IQuery {
  public:
    virtual ~IQuery() = default;
    virtual void refresh_chunks() = 0;

  protected:
    const std::vector<std::unique_ptr<IArchetype>>* archetypes_registry = nullptr;
};

template <typename... Filters>
    requires ValidQueryFilters<Filters...>
class Query : public IQuery {
  public:
    using IncludedComponents = typename meta::get_included_components<Filters...>::type;
    using ExcludedComponents = typename meta::get_excluded_components<Filters...>::type;

    Query(const std::vector<std::unique_ptr<IArchetype>>& archetypes) {
        ptrs.resize(max_id + 1, nullptr);
        archetypes_registry = &archetypes;
        refresh_chunks();
    }

    std::vector<Entity*> entities() {
        std::vector<Entity*> result;
        for (const auto& chunk : first_query().chunks)
            for (Entity& e : *chunk.entities) result.push_back(&e);
        return result;
    }

    void refresh_chunks() override {
        for (auto& a : *archetypes_registry) {
            const auto& desc = *a->descriptor;

            if (!matches(desc.components, inc_ids, exc_ids)) continue;

            a->visit_components([&](size_t id, void* vec) {
                if (id <= max_id) ptrs[id] = vec;
            });

            add_chunks<IncludedComponents>(a.get());
        }
    }

    size_t count() const {
        size_t total = 0;
        const auto& chunks = first_query().chunks;

        for (const auto& c : chunks) total += c.entities->size();
        return total;
    }

    struct iterator;

    iterator begin() const {
        auto& chunks = first_query().chunks;
        size_t c = 0;
        while (c < chunks.size() && chunks[c].data->empty()) ++c;
        return {this, c, 0};
    }

    iterator end() { return {this, first_query().chunks.size(), 0}; }

  private:
    template <typename Tuple>
    struct component_query_tuple;

    template <typename... Cs>
    struct component_query_tuple<std::tuple<Cs...>> {
        using type = std::tuple<ComponentQuery<Cs>...>;
    };

    using DataTuple = typename component_query_tuple<IncludedComponents>::type;
    DataTuple data;

    std::vector<void*> ptrs;

    inline auto& first_query() { return std::get<0>(data); }
    inline const auto& first_query() const { return std::get<0>(data); }

    template <typename Pack, size_t... Is>
    static const std::vector<size_t>& component_ids(std::index_sequence<Is...>) {
        static const std::vector<size_t> ids = [] {
            std::vector<size_t> v{component_id<typename std::tuple_element<Is, Pack>::type>()...};
            std::sort(v.begin(), v.end());
            return v;
        }();
        return ids;
    }

    inline static const auto& inc_ids =
        component_ids<IncludedComponents>(std::make_index_sequence<std::tuple_size_v<IncludedComponents>>{});
    inline static const auto& exc_ids =
        component_ids<ExcludedComponents>(std::make_index_sequence<std::tuple_size_v<ExcludedComponents>>{});
    inline static const size_t max_id = inc_ids.empty() ? 0 : inc_ids.back();

    bool contains_all(const std::vector<ComponentMover>& comps, const std::vector<size_t>& ids) const {
        size_t i = 0, j = 0;

        while (i < comps.size() && j < ids.size()) {
            if (comps[i].id < ids[j]) ++i;
            else if (comps[i].id == ids[j]) {
                ++i;
                ++j;
            } else return false;
        }

        return j == ids.size();
    }

    bool contains_any(const std::vector<ComponentMover>& comps, const std::vector<size_t>& ids) const {
        size_t i = 0, j = 0;

        while (i < comps.size() && j < ids.size()) {
            if (comps[i].id < ids[j]) ++i;
            else if (comps[i].id == ids[j]) return true;
            else ++j;
        }

        return false;
    }

    bool matches(
        const std::vector<ComponentMover>& comps, const std::vector<size_t>& inc, const std::vector<size_t>& exc
    ) const {
        if (!contains_all(comps, inc)) return false;
        if (contains_any(comps, exc)) return false;
        return true;
    }

    template <typename Pack, size_t... Is>
    void add_chunks_impl(IArchetype* a, std::index_sequence<Is...>) {
        (add_chunk<typename std::tuple_element<Is, Pack>::type>(
             a, std::get<ComponentQuery<typename std::tuple_element<Is, Pack>::type>>(data)
         ),
         ...);
    }

    template <typename Pack>
    void add_chunks(IArchetype* a) {
        add_chunks_impl<Pack>(a, std::make_index_sequence<std::tuple_size_v<Pack>>{});
    }

    template <typename C>
    void add_chunk(IArchetype* a, ComponentQuery<C>& cq) {
        void* p = ptrs[component_id<C>()];
        if (!p) return;

        if (std::any_of(cq.chunks.begin(), cq.chunks.end(), [&](auto& chunk) { return chunk.archetype == a; })) return;
        cq.chunks.emplace_back(a, static_cast<std::vector<C>*>(p), &a->entities);
    }

    // possibly redundant but could be used if a world could be "reset" to use
    // new archetypes template<std::size_t... Is> void
    // remove_chunks_impl(IArchetype* a, std::index_sequence<Is...>) {
    //     (remove_chunk<std::tuple_element_t<Is, IncludedComponents>>(a), ...);
    // }

    // void remove_chunks(IArchetype* a) {
    //     remove_chunks_impl(a,
    //     std::make_index_sequence<std::tuple_size_v<IncludedComponents>>{});
    // }

    // template<typename C>
    // void remove_chunk(IArchetype* a) {
    //     auto& chunks = std::get<ComponentQuery<C>>(data).chunks;
    //     chunks.erase(
    //         std::remove_if(chunks.begin(), chunks.end(),
    //             [&](auto& c) { return c.archetype == a; }
    //         ),
    //         chunks.end()
    //     );
    // }
};

template <typename... Filters>
    requires ValidQueryFilters<Filters...>
class Query<Filters...>::iterator {
  public:
    iterator() = default;
    iterator(const Query* q, size_t c, size_t i) : query(q), chunk(c), index(i) {
        if (chunk < std::get<0>(query->data).chunks.size()) load_chunk();
    }

    void load_chunk() {
        auto& c = std::get<0>(query->data).chunks[chunk];
        entity_data = c.entities->data();
        chunk_size = c.entities->size();

        component_data = load_component_data(std::make_index_sequence<std::tuple_size_v<IncludedComponents>>{});
    }

    auto operator*() const {
        return std::tuple_cat(
            std::tie(entity_data[index]),
            deref_component_data(std::make_index_sequence<std::tuple_size_v<IncludedComponents>>{})
        );
    }

    iterator& operator++() {
        ++index;
        if (index < chunk_size) return *this;

        ++chunk;
        index = 0;

        auto& chunks = std::get<0>(query->data).chunks;
        while (chunk < chunks.size()) {
            load_chunk();
            if (chunk_size != 0) break;
            ++chunk;
        }

        return *this;
    }

    friend bool operator==(const iterator& a, const iterator& b) {
        return a.query == b.query && a.chunk == b.chunk && a.index == b.index;
    }

    friend bool operator!=(const iterator& a, const iterator& b) { return !(a == b); }

  private:
    template <typename T>
    struct component_ptr_tuple;

    template <typename... Cs>
    struct component_ptr_tuple<std::tuple<Cs...>> {
        using type = std::tuple<Cs*...>;
    };

    const Query* query = nullptr;
    size_t chunk = 0;
    size_t index = 0;

    using DataTuple = typename component_ptr_tuple<IncludedComponents>::type;
    DataTuple component_data;

    Entity* entity_data = nullptr;

    size_t chunk_size = 0;

    template <std::size_t... Is>
    auto load_component_data(std::index_sequence<Is...>) {
        return std::tuple{std::get<Is>(query->data).chunks[chunk].data->data()...};
    }

    template <std::size_t... Is>
    auto deref_component_data(std::index_sequence<Is...>) const {
        return std::tuple<std::tuple_element_t<Is, IncludedComponents>&...>{std::get<Is>(component_data)[index]...};
    }
};

} // namespace rw_ecs