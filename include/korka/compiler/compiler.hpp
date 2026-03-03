#pragma once

#include "korka/shared/error.hpp"
#include "korka/shared/flat_map.hpp"
#include "korka/utils/overloaded.hpp"
#include "parser.hpp"
#include "korka/vm/bytecode_builder.hpp"
#include <ranges>
#include <vector>
#include <optional>
#include <string_view>
#include <flat_map>

namespace korka {
  struct void_t {
  };

  using type_info = std::variant<korka::type>;

  constexpr auto string_to_type(std::string_view name) -> type {
    if (name == "int") return type::i64;
    else if (name == "void") return type::void_;
    // TODO: other types
    return type::i64;
  }

  constexpr auto type_to_string(type t) -> std::string_view {
    switch (t) {
      case type::void_:
        return "void";
      case type::i64:
        return "int";
    }
  }

  struct variable_info {
    std::string_view name;
    type_info type;
    // bp + stack_offset
    std::size_t stack_offset;

    static constexpr auto from_node(const nodes::decl_var &node) -> variable_info {
      return {
        .name = node.var_name,
        .type{},
        .stack_offset{}
      };
    }
  };

  struct function_info {
    std::string_view name;
    std::vector<variable_info> params;
    type_info return_type;

    vm::bytecode_builder::label label;
  };

  struct symbol_table {
    struct scope {
      flat_map<std::string_view, variable_info> variables;
      std::size_t current_stack_frame_size = 0;
    };
    std::vector<scope> scopes;
    flat_map<std::string_view, function_info> functions;

    constexpr auto push_scope() -> void { scopes.emplace_back(); }

    constexpr auto pop_scope() -> void { scopes.pop_back(); }

    constexpr auto declare_var(std::string_view name, const type_info &type) -> std::expected<variable_info, error_t> {
      if (scopes.empty())
        return std::unexpected{error::other_compiler_error{
          .message = "No scope"
        }};

      auto &current = scopes.back();
      if (current.variables.contains(name))
        return std::unexpected{error::redeclaration{
          .identifier = name
        }};

      variable_info info{
        .name = name,
        .type = type,
        .stack_offset = current.current_stack_frame_size
      };

      // For now, assume every variable (i64) is 8 bytes
      current.current_stack_frame_size += 8;
      current.variables[name] = info;
      return info;
    }

    constexpr auto declare_function(std::string_view name, auto &&...args) -> std::expected<void, error_t> {
      functions.emplace(std::piecewise_construct,
                        std::forward_as_tuple(name),
                        std::forward_as_tuple(name, std::forward<decltype(args)>(args)...));
      return {};
    }

    constexpr auto lookup_variable(std::string_view name) -> std::optional<variable_info> {
      for (auto &scope: std::ranges::reverse_view(scopes)) {
        if (auto var_it = scope.variables.find(name); var_it != std::end(scope.variables)) {
          return var_it->second;
        }
      }
      return std::nullopt;
    }

    constexpr auto lookup_function(std::string_view name) -> std::optional<function_info> {
      if (auto func_it = functions.find(name); func_it != std::end(functions)) {
        return func_it->second;
      }
      return std::nullopt;
    }

    constexpr auto clear() -> void {
      scopes.clear();
      functions.clear();
    }
  };

  struct static_symbol_table {

    template<auto &&symbol_table_getter>
    static constexpr auto make_from_table() -> static_symbol_table {
      constexpr static symbol_table table = symbol_table_getter();
    }
  };

  class compiler {
  public:
    constexpr compiler(std::span<const nodes::node> nodes, nodes::index_t root_node)
      : m_nodes(nodes), m_root_node(root_node) {}

    constexpr auto compile() -> std::expected<std::vector<std::byte>, error_t> {
      m_symbols.push_scope();
      auto ok = process_node(m_root_node);
      if (!ok) return std::unexpected{ok.error()};

      return builder.build();
    }

  private:
    std::span<const nodes::node> m_nodes;
    nodes::index_t m_root_node;
    symbol_table m_symbols;
    vm::bytecode_builder builder;

    // Info for ast walker
    std::optional<type_info> m_current_func_ret;

    using result_t = std::expected<type_info, error_t>;

    constexpr auto process_node(nodes::index_t idx) -> result_t {
      const auto &node = m_nodes[idx];

      return std::visit(overloaded{
        [&](const nodes::decl_program &program) -> result_t {
          for (auto item: nodes::get_list_view(m_nodes, program.external_declarations_head)) {
            auto ok = process_node(item);
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
                                   .stack_offset = 0 // Will be calculated inside the scope
                                 });
          }

          // Register the function globally BEFORE processing the body
          // This allows the function to "see" itself (recursion)
          auto reg_ok = m_symbols.declare_function(
            function.name,
            parameters,
            ret_type,
            label
          );
          if (not reg_ok) return std::unexpected{reg_ok.error()};

          // Entering function scope
          m_symbols.push_scope();
          m_current_func_ret = ret_type;
          builder.bind_label(label);

          // Handle parameters as local variables
          for (auto &&param: parameters) {
            m_symbols.declare_var(param.name, param.type);
          }

          // Function body
          for (auto stmt: nodes::get_list_view(m_nodes, function.body)) {
            if (auto res = process_node(stmt); !res) {
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
              builder.emit_const<type::i64>(val);
              return type_info{type::i64};
            }
            return std::unexpected{
              error::other_compiler_error{.message = "This type is not supported as a literal yet"}};
          }, lit);
        },
        [&](const nodes::stmt_block &block) -> result_t {
          for (auto stmt: nodes::get_list_view(m_nodes, block.children_head)) {
            if (auto res = process_node(stmt); !res) return res;
          }
          return {};
        },

        [&](const nodes::stmt_return &stmt) -> result_t {
          auto actual_type = process_node(stmt.expr);
          if (!actual_type) return actual_type;

          // Semantic Check: Does return type match function signature?
          if (m_current_func_ret && *actual_type != *m_current_func_ret) {
            // TODO: proper error
            return std::unexpected{error::other_compiler_error{
              .message = "Function return type mismatch"
            }};
          }

          builder.emit_op(vm::op_code::ret);
          return *actual_type;
        },

        [&](const nodes::expr_var &var) -> result_t {
          auto info = m_symbols.lookup_variable(var.name);
          if (!info)
            return std::unexpected{error::undefined_symbol{
              .identifier = var.name
            }};

          // TODO: emit_load_local(info->stack_offset)
          return info->type;
        },
        [&](const auto &value) -> result_t {
          std::ignore = value;
          return std::unexpected{error::other_compiler_error{
            "Not implemented"
          }};
        }
      }, node.data);
    }
  };
} // namespace korka