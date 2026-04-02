#pragma once
#include "korka/vm/context_base.hpp"

namespace korka {
  using vm_external_function_id = std::uint16_t;
  using vm_external_function_type = void(vm::context_base &context);

  template<auto Func>
  auto binding_wrapper(vm::context_base &ctx) -> void {
    using function_type = std::remove_cvref_t<decltype(Func)>;
    using traits = function_traits<function_type>;
    using rtype = typename traits::return_type;
    using args_tuple = typename traits::args_tuple;
    constexpr auto arg_count = traits::args_count;

    // Load args from stack
    std::array<vm::value, arg_count> retrieved_args;
    for (int i = arg_count - 1; i >= 0; --i) {
      retrieved_args[i] = ctx.pop_value();
    }

    // Unbox them into types
    auto args = [&]<std::size_t ...I>(std::index_sequence<I...>) {
      return args_tuple{
        vm::unbox<std::tuple_element_t<I, args_tuple>>(retrieved_args[I])...
      };
    }(std::make_index_sequence<arg_count>());

    if constexpr (std::is_void_v<rtype>) {
      std::apply(Func, std::move(args));
    } else {
      rtype v = std::apply(Func, std::move(args));
      auto boxed = vm::box(v);
      ctx.push_value(boxed);
    }
  }
}