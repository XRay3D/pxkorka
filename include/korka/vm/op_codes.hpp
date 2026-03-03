#pragma once

#include <cstdint>
#include "korka/shared/types.hpp"

namespace korka::vm {
  enum class op_code {
    // Pushes a value onto the stack
    i64_const, // <op><i64>

    ret
  };

  template<korka::type Type>
  constexpr op_code get_const_op_by_type() {
    if constexpr (Type == korka::type::i64) {
      return op_code::i64_const;
    } else {
      static_assert(false, "Unknown type");
    }
  }

  constexpr int op_code_size = 1;
}