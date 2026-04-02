#pragma once

#include <frozen/bits/elsa.h>
#include "frozen/unordered_map.h"
#include "korka/compiler/binding.hpp"
#include "korka/shared/error.hpp"
#include "korka/shared/flat_map.hpp"
#include "korka/utils/overloaded.hpp"
#include "korka/vm/op_codes.hpp"
#include "parser.hpp"
#include "korka/vm/bytecode_builder.hpp"
#include "korka/utils/frozen_hash_string_view.hpp"
#include <ranges>
#include <vector>
#include <optional>
#include <string_view>
#include "info.hpp"
#include "binding.hpp"
#include "result.hpp"

namespace korka {
  template<std::size_t BindingsCount = 0, std::size_t BindingMaxParamCount = 0>
  class compiler {
    using bindings_container = bindings<BindingsCount, BindingMaxParamCount>;

  public:
    constexpr compiler(std::span<const nodes::node> nodes, nodes::index_t root_node, const bindings_container &bindings)
      : m_nodes(nodes), m_root_node(root_node), m_bindings(bindings) {
    }

    constexpr auto compile() -> std::expected<compilation_result, error_t> {
      m_symbols.push_scope();
      auto ok = process_node(m_root_node);
      if (!ok) return std::unexpected{ok.error()};

      return compilation_result{
        builder.build(),
        m_symbols.functions
      };
    }

  private:
    std::span<const nodes::node> m_nodes;
    nodes::index_t m_root_node;
    symbol_table m_symbols;
    vm::bytecode_builder builder;

    // External bindings
    bindings_container m_bindings;

    // Info for ast walker
    std::optional<type_info> m_current_func_ret;

    using result_t = std::expected<type_info, error_t>;

    constexpr auto process_node(nodes::index_t idx, bool emit = true) -> result_t {
      const auto &node = m_nodes[idx];

      return std::visit(overloaded{
        [&](const nodes::decl_program &program) -> result_t {
          for (auto item: nodes::get_list_view(m_nodes, program.external_declarations_head)) {
            auto ok = process_node(item, emit);
            if (not ok) {
              return std::unexpected{ok.error()};
            }
          }

          return type_info{type::void_};
        },
        [&](const nodes::decl_function &function) -> result_t {
          type_info ret_type = string_to_type(function.ret_type);
          m_current_func_ret = ret_type;

          // Function pointer
          auto label = builder.make_label();

          // Collect parameter info for the function signature
          std::vector<variable_info> parameters;
          for (auto p_idx: nodes::get_list_view(m_nodes, function.params_head)) {
            const auto &p_node = std::get<nodes::decl_var>(m_nodes[p_idx].data);
            parameters.push_back({
                                   .name = p_node.var_name,
                                   .type = string_to_type(p_node.type_name),
                                   .locals_index = 0
                                 });
          }

          // Register the function globally BEFORE processing the body
          // This allows the function to "see" itself (recursion)

          builder.bind_label(label);
          auto func_start_pos = builder.resolve_label(label);
          if (not func_start_pos) return std::unexpected{error::other_compiler_error{"Idk"}};

          auto reg_ok = m_symbols.declare_function(
            function.name,
            parameters,
            ret_type,
            *func_start_pos
          );
          if (not reg_ok) return std::unexpected{reg_ok.error()};

          // Entering function scope
          m_symbols.push_scope();
          m_current_func_ret = ret_type;

          // Handle parameters as local variables
          for (auto &&param: parameters) {
            auto ok = m_symbols.declare_var(param.name, param.type);
            if (not ok) {
              return std::unexpected{ok.error()};
            }
          }

          // Function body
          for (auto stmt: nodes::get_list_view(m_nodes, function.body)) {
            if (auto res = process_node(stmt, emit); !res) {
              m_symbols.pop_scope(); // clean up
              return res;
            }
          }

          // cleanup
          m_symbols.pop_scope();
          m_current_func_ret.reset();

          return {};
        },

        [&](const nodes::expr_literal &lit) -> result_t {
          return std::visit([&](auto val) -> result_t {
            using T = decltype(val);
            if constexpr (std::is_same_v<T, std::int64_t>) {
              if (emit) {
                builder.emit_const<type::i64>(val);
              }
              return type_info{type::i64};
            }
            return std::unexpected{
              error::other_compiler_error{.message = "This type is not supported as a literal yet"}};
          }, lit);
        },
        [&](const nodes::stmt_block &block) -> result_t {
          for (auto stmt: nodes::get_list_view(m_nodes, block.children_head)) {
            if (auto res = process_node(stmt, emit); !res) return res;
          }
          return {};
        },

        [&](const nodes::stmt_return &stmt) -> result_t {
          if (stmt.expr != empty_node) {
            auto actual_type = process_node(stmt.expr, emit);
            if (!actual_type) return actual_type;

            // Semantic Check: Does return type match function signature?
            if (m_current_func_ret && *actual_type != *m_current_func_ret) {
              // TODO: proper error
              return std::unexpected{error::other_compiler_error{
                .message = "Function return type mismatch"
              }};
            }
            if (emit) {
              builder.emit_op(vm::op_code::ret);
            }
            return *actual_type;
          } else {
            if (emit) {
              builder.emit_op(vm::op_code::ret);
            }
            return korka::type_info{korka::type::void_};
          }
        },
        [&](const nodes::decl_var &var) -> result_t {
          auto ok = m_symbols.declare_var(var.var_name, string_to_type(var.type_name));
          if (!ok) {
            return std::unexpected{ok.error()};
          }

          if (var.init_expr != nodes::empty_node) {
            auto expr = process_node(var.init_expr, emit);
            if (not expr) {
              return expr;
            }
            if (emit) {
              builder.emit_save_local(ok->locals_index);
            }
          }
          return ok->type;
        },

        [&](const nodes::expr_var &var) -> result_t {
          auto info = m_symbols.lookup_variable(var.name);
          if (!info) {
            return std::unexpected{error::undefined_symbol{
              .identifier = var.name
            }};
          }

          if (emit) {
            builder.emit_load_local(info->locals_index);
          }
          return info->type;
        },
        [&](const nodes::expr_binary &expr) -> result_t {
          auto left = process_node(expr.left, emit);
          if (not left) {
            return left;
          }
          auto right = process_node(expr.right, emit);
          if (not right) {
            return right;
          }

          if ((*left) != (*right)) {
            return std::unexpected{error::other_compiler_error{
              "Expected same types in the binary expression"
            }};
          }

          using namespace std::literals;
          constexpr std::array comparison_ops = {
            "=="sv
          };


          if (emit) {
            if (std::ranges::contains(comparison_ops, expr.op)) {
              if (auto *type = std::get_if<korka::type>(&*left)) {
                switch (*type) {
                  case type::void_:
                    return std::unexpected{error::other_compiler_error{"wtf"}};
                  case type::i64: {
                    auto &op = expr.op;
                    if (op == "==") {
                      builder.emit_op(vm::op_code::i64_cmp);
                    } else {
                      return std::unexpected{error::other_compiler_error{"Unsupported operation"}};
                    }
                    break;
                  }
                }
              } else {
                return std::unexpected{error::other_compiler_error{"Unsupported type"}};
              }
            } else {
              // Basic math
              auto code = vm::get_op_code_for_math(*left, *right, expr.op);
              if (not code) {
                return std::unexpected{code.error()};
              }
              builder.emit_op(*code);
            }
          }

          return *left;
        },

        [&](const nodes::stmt_if &if_) -> result_t {
          auto condition_expr = process_node(if_.condition, emit);
          if (not condition_expr) {
            return condition_expr;
          }

          auto else_branch_label = builder.make_label();
          auto end_label = builder.make_label();

          if (if_.else_branch == nodes::empty_node) {
            builder.emit_jmp_if_zero(end_label);

            auto then_branch = process_node(if_.then_branch, emit);
            if (not then_branch) {
              return then_branch;
            }
          } else {
            builder.emit_jmp_if_zero(else_branch_label);

            auto then_branch = process_node(if_.then_branch, emit);
            if (not then_branch) {
              return then_branch;
            }

            builder.emit_jmp(end_label);

            builder.bind_label(else_branch_label);
            auto else_branch = process_node(if_.else_branch);
            if (not else_branch) {
              return else_branch;
            }
          }

          builder.bind_label(end_label);
          return {};
        },
        [&](nodes::expr_call const &call) -> result_t {
          auto func = m_symbols.lookup_function(call.name);
          auto bindings_func = m_bindings.get(call.name);


          if (not func and not bindings_func) {
            return std::unexpected{error::undefined_symbol{.identifier = call.name}};
          }

          std::vector<nodes::index_t> arg_indices;

          // Internal function
          if (func) {
            auto &expected_params = func->params;

            std::size_t pidx{};
            for (auto param_node_idx: nodes::get_list_view(m_nodes, call.args_head)) {
              auto param_type = process_node(param_node_idx, /*emit=*/ false);
              if (!param_type) return param_type;

              if (pidx >= expected_params.size() || expected_params[pidx].type != *param_type) {
                return std::unexpected{error::function_call_param_mismatch{
                  .function_name = call.name, .param_idx = pidx
                }};
              }
              arg_indices.emplace_back(param_node_idx);
              pidx++;
            }
          } else { // External function
            auto &expected_params = bindings_func->param_types;

            std::size_t pidx{};
            for (auto param_node_idx: nodes::get_list_view(m_nodes, call.args_head)) {
              auto param_type = process_node(param_node_idx, false);
              if (not param_type) return param_type;

              if (pidx >= expected_params.size() or expected_params[pidx] != *param_type) {
                return std::unexpected{error::function_call_param_mismatch{
                  .function_name = call.name, .param_idx = pidx
                }};
              }

              arg_indices.emplace_back(param_node_idx);
              pidx++;
            }
          }

          if (emit) {
            for (auto idx: arg_indices) {
              if (auto ok = process_node(idx, true); !ok) return ok;
            }

            if (func) {
              builder.emit_const<korka::type::i64>(static_cast<int64_t>(arg_indices.size()));
              builder.emit_call(func->start_pos);
            } else {
              builder.emit_trap(bindings_func->id);
            }
          }

          if (func) {
            return func->return_type;
          } else {
            return bindings_func->return_type;
          }
        },
        [&](const stmt_expr &expr) -> result_t {
          return process_node(expr.expr, emit);
        },

        [&](const auto &value) -> result_t {
          throw 0;
          std::ignore = value;
          return std::unexpected{error::other_compiler_error{
            "Not implemented"
          }};
        }
      }, node.data);
    }
  };

  template<auto &&nodes, nodes::index_t root, auto bindings>
  consteval static auto compile_nodes() {

    constexpr static auto expected = [] { return compiler{nodes, root, *bindings}.compile(); };

    if constexpr (not expected()) {
      report_error<[] { return expected().error(); }>();
      return expected().error();
    } else {
      return compilation_result_to_const<[] constexpr { return expected().value(); }>();
    }
  }

  template<const_string code, auto bindings>
  consteval static auto compile() {
    constexpr static auto nodes_root = parse<code>();

    return compile_nodes<nodes_root.first, nodes_root.second, bindings>();
  }
} // namespace korka