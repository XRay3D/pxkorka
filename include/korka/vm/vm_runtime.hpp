//
// Created by pyxiion on 29.01.2026.
//

#pragma once

#include "korka/utils/function_traits.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/utils/byte_reader.hpp"
#include "korka/compiler/compiler.hpp"
#include <array>
#include <bit>
#include <vector>
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

  constexpr auto box_int(std::int64_t i) -> value {
    return std::bit_cast<value>(i);
  }

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

  class context {
  public:
    explicit context(std::span<const std::byte> bytes)
      : m_reader(bytes) {
      m_stack.reserve(64);
    }

    template<class Signature, class ...Args, class Traits = function_traits<Signature>>
    auto call(function_runtime_info_with_signature<Signature> func, Args &&...args) -> typename Traits::return_type {
      format_static_assert<Traits::args_count == sizeof...(args), [] {
        return format("This function requires ~ arguments, got ~", Traits::args_count, sizeof...(args));
      }>();

      // Create a scope for the call
      push_scope();
      // TODO: check argument types

      // Pass arguments
      std::size_t i = 0;
      ([&]() {
        current_scope()->set_local_value(i++, box(args));
      }(), ...);

      m_reader.set_cursor(func.start_pos);

      // Run until ret
      // Check scopes size to ensure it's the original function's return
      while (true) {
        auto op = execute_op();

        if (op == op_code::ret && m_scopes.empty()) {
          m_scopes.clear();
          break;
        }
      }

      return pop<typename Traits::return_type>();
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

  private:
    auto execute_op() -> op_code {
      auto initial_pc = m_reader.cursor();

      const auto code = m_reader.read<op_code>();

      switch (code) {
        case op_code::lload: {
          const auto index = m_reader.read<local_index_t>();
          push_value(current_scope()->get_local_value(index));
        }
          break;
        case op_code::pload:
          throw std::runtime_error("Not implemented");
          break;
        case op_code::lsave: {
          const auto index = m_reader.read<local_index_t>();
          current_scope()->set_local_value(index, pop_value());
        }
          break;
        case op_code::i64_const: {
          const auto v = m_reader.read<std::int64_t>();
          push(v);
        }
          break;
        case op_code::i64_add: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(a + b);
        }
          break;
        case op_code::i64_sub: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(a - b);
        }
          break;
        case op_code::i64_mul: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(a * b);
        }
          break;
        case op_code::i64_div: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(a / b);
        }
          break;
        case op_code::i64_cmp: {
          const auto b = pop<std::int64_t>();
          const auto a = pop<std::int64_t>();
          push(static_cast<std::int64_t>(a == b));
        }
          break;
        case op_code::jmp: {
          const auto offset = m_reader.read<jump_offset>();
          m_reader.set_cursor(initial_pc + offset);
        }
          break;
        case op_code::jmpz: {
          const auto offset = m_reader.read<jump_offset>();
          auto v = pop<std::int64_t>();
          if (v == 0) {
            m_reader.set_cursor(initial_pc + offset);
          }
        }
          break;
        case op_code::call: {
          auto called_address = m_reader.read<address_t>();
          current_scope()->suspension_point = m_reader.cursor();
          auto arg_count = pop<type::i64>();

          push_scope();

          // Load args
          for (int i = arg_count - 1; i >= 0; --i) {
            auto v = pop_value();
            current_scope()->set_local_value(i, v);
          }

          m_reader.set_cursor(called_address);
        }
          break;
        case op_code::ret:
          pop_scope();
          if (current_scope()) {
            m_reader.set_cursor(current_scope()->suspension_point);
          }
          break;
      }

      return code;
    }

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

  class runtime {
  public:
//    auto create_context() -> context {
//      return {};
//    }


  };

} // korka::vm