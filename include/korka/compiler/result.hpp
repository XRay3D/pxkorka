//
// Created by pyxiion on 20/03/2026.
//
#pragma once

#include <array>
#include "korka/vm/op_codes.hpp"
#include "korka/compiler/info.hpp"
#include "korka/shared/flat_map.hpp"
#include "korka/utils/frozen_hash_string_view.hpp"

namespace korka {
  template<auto info_getter>
  struct _extract_function_signature {
    template<typename IndexSequence>
    struct param_helper;

    template<std::size_t... Is>
    struct param_helper<std::index_sequence<Is...>> {
      // Map each parameter info to its corresponding C++ type
      using type = vm::type_info_to_cpp_t<[] { return info_getter().return_type; }>(
        vm::type_info_to_cpp_t<[] { return info_getter().params[Is].type; }>...
      );
    };

    using type = typename param_helper<
      std::make_index_sequence<info_getter().param_count>
    >::type;
  };

  template<auto info_getter>
  using const_function_info_to_signature_t = typename _extract_function_signature<info_getter>::type;

  template<std::size_t NMaxParams>
  constexpr auto function_info_to_const(const function_info &f) {
    const_function_info<NMaxParams> info{
      .name = f.name,
      .param_count = f.params.size(),
      .params{},
      .return_type = f.return_type,
      .start_pos = f.start_pos
    };

    std::ranges::copy(f.params, std::begin(info.params));
    return info;
  }

  struct symbol_table {
    struct scope {
      flat_map<std::string_view, variable_info> variables;

      std::size_t current_locals_size{};
    };
    std::vector<scope> scopes;
    flat_map<std::string_view, function_info> functions;

    constexpr auto push_scope() -> void { scopes.emplace_back(); }

    constexpr auto pop_scope() -> void { scopes.pop_back(); }

    constexpr auto declare_var(std::string_view name, const type_info &type) -> std::expected<variable_info, error_t> {
      if (scopes.empty()) {
        return std::unexpected{error::other_compiler_error{
          .message = "No scope"
        }};
      }

      auto &current = scopes.back();
      if (current.variables.contains(name)) {
        return std::unexpected{error::redeclaration{
          .identifier = name
        }};
      }

      variable_info info{
        .name = name,
        .type = type,
        .locals_index = current.current_locals_size++
      };

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
      for (auto &scp: std::ranges::reverse_view(scopes)) {
        if (auto var_it = scp.variables.find(name); var_it != std::end(scp.variables)) {
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


  struct compilation_result {
    std::vector<std::byte> bytes;
    flat_map<std::string_view, function_info> functions;
  };


  struct function_runtime_info {
    std::size_t start_pos;
  };

  template<class Signature>
  struct function_runtime_info_with_signature : public function_runtime_info {
    using signature_t = Signature;

    using function_runtime_info::start_pos;
  };

  template<std::size_t NBytes, std::size_t NFunctions, std::size_t NMaxParams, class SignatureMapper>
  struct const_compilation_result {
    std::array<std::byte, NBytes> bytes;
    frozen::unordered_map<std::string_view, const_function_info<NMaxParams>, NFunctions> functions;

    template<const_string name>
    using get_signature_t = typename SignatureMapper::template get_signature_t<name>;

    template<const_string name>
    constexpr auto function() const {
      return function_runtime_info_with_signature<get_signature_t<name>>{
        functions.at(name).start_pos
      };
    }
  };

  template<auto>
  struct unique_type {
  };

  template<auto function_info_getter, typename IndexSequence>
  struct signature_mapper;

  template<auto function_info_getter, std::size_t... Is>
  struct signature_mapper<function_info_getter, std::index_sequence<Is...>> {
    consteval static auto hash(auto &&v) -> std::size_t {
      return frozen::elsa<std::string_view>{}(v, 0);
    }

    constexpr static auto _overloaded = overloaded{
      ([](unique_type<hash(function_info_getter(Is).name)>)
        -> const_function_info_to_signature_t<[] { return function_info_getter(Is); }> * {
        return nullptr;
      })...
    };

    // Retrieve the type
    template<const_string name>
      #ifdef __clang__ // GCC crashes on this check for some reason
      requires ([] -> bool {
        constexpr bool found = requires {
          { _overloaded(unique_type<hash(name)>{}) };
        };
        if constexpr (not found) {
          report_error<[] {
            return format("Symbol '~' not found", name);
          }>();
        }
        return true;
      }())
      #endif
    using get_signature_t = std::remove_pointer_t<decltype(_overloaded(
      unique_type<hash(name)>{}))>;
  };


  template<auto r>
  constexpr auto compilation_result_to_const() {
    // --- BYTES ---
    constexpr static auto bytes = to_array<[] { return r().bytes; }>();

    // --- FUNCTIONS ---
    constexpr static auto function_count = []() constexpr {
      return r().functions.size();
    }();
    constexpr static auto max_params_n = []() constexpr {
      std::size_t n{};
      for (auto &&f: r().functions) {
        n = std::max(n, f.second.params.size());
      }
      return n;
    }();
    constexpr static auto functions = []() constexpr {
      std::array<std::pair<std::string_view, const_function_info<max_params_n>>, function_count> functions_data{};
      std::size_t i{};
      for (auto &&[key, value]: r().functions) {
        functions_data[i++] = (std::make_pair(key, function_info_to_const<max_params_n>(value)));
      }
      return frozen::make_unordered_map(functions_data);
    };

    // Function mapper
    using sign_mapper = signature_mapper<[](std::size_t i) {
      return (functions().begin() + i)->second;
    }, std::make_index_sequence<function_count>>;

    return const_compilation_result<bytes.size(), function_count, max_params_n, sign_mapper>{
      bytes,
      functions()
    };
  }
}