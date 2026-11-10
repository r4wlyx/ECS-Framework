#pragma once

namespace rw_meta {

template <typename T>
consteval const char* type_name() {
#ifdef __clang__
    return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
    return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
    return __FUNCSIG__;
#endif
}

consteval size_t hash_ctti(const char* str) {
    size_t h = 0xcbf29ce484222325;
    while (*str) {
        h ^= static_cast<size_t>(*str++);
        h *= 0x100000001b3;
    }
    return h;
}

template <typename T>
consteval size_t type_id_v() {
    return hash_ctti(type_name<T>());
}

template <typename A, typename B>
struct type_id_less {
    static constexpr bool value = type_id_v<A>() < type_id_v<B>();
};

template <typename A, typename B>
constexpr bool type_id_less_v = type_id_less<A, B>::value;

} // namespace rw_meta