#pragma once

#include <string_view>
#include "korka/shared/error.hpp"
#include <frozen/bits/elsa.h>
#include "korka/vm/op_codes.hpp"
#include "korka/compiler/parser.hpp"

namespace korka {

  using vm::type_info;

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
    std::string_view name{};
    type_info type;

    std::size_t locals_index{};

    static constexpr auto from_node(const nodes::decl_var &node) -> variable_info {
      return {
        .name = node.var_name,
        .type{},
        .locals_index{}
      };
    }
  };

  struct function_info {
    std::string_view name;
    std::vector<variable_info> params;
    type_info return_type;

    std::size_t start_pos;
  };

  template<std::size_t NMaxParams>
  struct const_function_info {
    std::string_view name{};
    std::size_t param_count{};
    std::array<variable_info, NMaxParams> params{};
    type_info return_type{};

    std::size_t start_pos{};
  };
}