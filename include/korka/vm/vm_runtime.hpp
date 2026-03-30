//
// Created by pyxiion on 29.01.2026.
//

#pragma once

#include "korka/utils/function_traits.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/utils/byte_reader.hpp"
#include "korka/compiler/result.hpp"
#include "korka/vm/value.hpp"
#include "korka/compiler/binding.hpp"
#include "korka/vm/context_base.hpp"
#include <array>
#include <bit>
#include <vector>
#include <cstdint>

namespace korka::vm {
  template<bindings_concepts bindings_t = bindings<0, 0>>
  class context : public context_base {
  public:
    explicit context(std::span<const std::byte> bytes, bindings_t binds = {})
      : context_base(bytes), m_bindings(binds) {

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

      using return_type = typename Traits::return_type;
      if constexpr (std::is_void_v<return_type>) {
        return;
      } else {
        return pop<return_type>();
      }
    }

  protected:
    bindings_t m_bindings;

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
        case op_code::trap: {
          auto id = m_reader.read<vm_external_function_id>();
          auto func = m_bindings.get_callable_by_id(id);
          (*func)(*this);
        }
          break;
      }

      return code;
    }
  };

  class runtime {
  public:
//    auto create_context() -> context {
//      return {};
//    }


  };

} // korka::vm