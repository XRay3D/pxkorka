//
// Created by pyxiion on 20/03/2026.
//

#pragma once
#include <array>
#include <cstdint>

namespace korka::vm {
  using value = std::array<std::byte, 8>;

  template<class T>
  constexpr auto box(T const &v) -> value {
    static_assert(std::is_same_v<T, std::int64_t> or std::is_same_v<T, double>, "T must be int or double");

    return std::bit_cast<value>(v);
  }

  template<class T>
  constexpr auto unbox(value const &v) -> T {
    static_assert(std::is_same_v<T, std::int64_t> or std::is_same_v<T, double>, "T must be int or double");

    return std::bit_cast<T>(v);
  }
}