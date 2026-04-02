//
// Created by pyxiion on 09.03.2026.
//

#pragma once

#include <tuple>

namespace korka {
  template<class T>
  struct function_traits;

  template<class R, class ...Args>
  struct function_traits<R(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;

    constexpr static std::size_t args_count = sizeof...(Args);

    /**
     * callback(Args*... = nullptr)
     */
    static constexpr auto process_args(auto &&callback) {
      callback(std::add_pointer_t<Args>{}...);
    }
  };

  template<typename R, typename... Args>
  struct function_traits<R(*)(Args...)> : function_traits<R(Args...)> {

  };
}