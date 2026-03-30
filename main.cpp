#include "korka/compiler/binding.hpp"
#include "korka/compiler/parser.hpp"
#include "korka/compiler/compiler.hpp"
#include "korka/compiler/ast_walker.hpp"
#include "korka/vm/vm_runtime.hpp"
#include <print>
#include <iostream>

auto fib(std::int64_t n) -> std::int64_t {
  if (n == 0) return 0;
  if (n == 1) return 1;

  return fib(n - 1) + fib(n - 2);
}

auto print_n(std::int64_t n) -> void {
  std::cout << n << '\n';
}

constexpr char code[] = R"(
int fib(int n) {
  if (n == 0) return 0;
  if (n == 1) return 1;

  return fib(n-1) + cpp_fib(n-2);
}

void print_fib(int n) {
  int result = fib(n);

  print_n(result);
  return;
}
)";

constexpr auto bindings = korka::make_bindings(
  korka::wrap<fib>("cpp_fib"),
  korka::wrap<print_n>("print_n")
);

constexpr auto compile_result = korka::compile<code, &bindings>();

constexpr auto script_fib = compile_result.function<"fib">();
constexpr auto script_print_fib = compile_result.function<"print_fib">();

int main() {
  korka::vm::context ctx{compile_result.bytes, bindings};

  auto result = ctx.call(script_fib, 12L);
  std::cout << result << '\n';

  ctx.call(script_print_fib, 16L);

//  std::println("{}", ctx.call(fib_func, 12L));

//  std::ignore = tokens;
//  std::println("{:n:02X}", compile_result.bytes | std::views::transform([](auto b) { return static_cast<int>(b); }));

//  auto lexed = korka::lexer{code}.lex();
//  if (not lexed) {
//    std::println("{}", korka::to_string(lexed.error()));
//    return 0;
//  }
//
//  auto parsed = korka::parser{lexed.value()}.parse();
//  if (not parsed) {
//    std::println("{}", korka::to_string(parsed.error()));
//    return 0;
//  }
//  auto [node_pool, node_root] = parsed.value();
//  std::println("{}", korka::ast_walker{node_pool, node_root, 0});
//
//  korka::compiler compiler{node_pool, node_root};
//  auto bytes = compiler.compile();
//
//  if (bytes) {
//    std::println("{::X}", *bytes | std::views::transform([](auto b) { return static_cast<int>(b); }));
//  } else {
//    std::println("{}", korka::to_string(bytes.error()));
//  }

}