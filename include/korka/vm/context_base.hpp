#pragma once

#include "korka/utils/function_traits.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/utils/byte_reader.hpp"
#include "korka/compiler/result.hpp"
#include "korka/vm/value.hpp"
#include "korka/compiler/binding.hpp"
#include <array>
#include <bit>
#include <vector>
#include <cstdint>

namespace korka::vm {
  template<class T>
  concept bindings_concepts = requires(const T &t) {
    { t.get_callable_by_id(0) };
  };

  struct function_scope {
    // Set on `call`, used on `ret`
    std::size_t suspension_point{};

    std::vector<value> locals{8};

    auto set_local_value(local_index_t i, value const &v) -> void {
      locals.at(i) = v;
    }

    template<class T = value>
    auto set_local(local_index_t i, T const &value) -> void {
      set_local_value(i, box<T>(value));
    }

    auto get_local_value(local_index_t i) -> value {
      return locals.at(i);
    }

    template<class T = value>
    auto get_local(local_index_t i) -> T {
      return unbox<T>(get_local_value(i));
    }


    auto clear() -> void {
      locals.clear();
    }
  };

  class context_base {
  public:
    explicit context_base(std::span<const std::byte> bytes)
      : m_reader(bytes) {
      m_stack.reserve(64);
    }

    auto push_value(value const &v) -> void {
      m_stack.emplace_back(v);
    }

    template<class T>
    auto push(T const &v) -> void {
      push_value(box<T>(v));
    }

    auto pop_value() -> value {
      value v = m_stack.back();
      m_stack.pop_back();
      return v;
    }

    template<class T>
    auto pop() -> T {
      return unbox<T>(pop_value());
    }

    template<korka::type T>
    auto pop() -> type_to_cpp_t<T> {
      return unbox<type_to_cpp_t<T>>(pop_value());
    }

  protected:

    auto current_scope() -> function_scope * {
      return m_scopes.empty() ? nullptr : std::addressof(m_scopes.back());
    }

    auto push_scope() -> void {
      m_scopes.emplace_back();
    }

    auto pop_scope() -> void {
      m_scopes.pop_back();
    }

    std::vector<value> m_stack;
    std::vector<function_scope> m_scopes;

    byte_reader m_reader;
  };
}