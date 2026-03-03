#include "korka/compiler/parser.hpp"
#include "korka/compiler/compiler.hpp"
#include "korka/compiler/ast_walker.hpp"
#include <print>

constexpr char code[] = R"(
int main() {
  return 5;
}
)";

//constexpr static auto tokens = korka::lex<code>();

//constexpr auto ast = korka::parse<code>();
//
//constexpr auto node_pool = ast.first;
//constexpr auto node_root = ast.second;

int main() {
  auto lexed = korka::lexer{code}.lex();
  if (not lexed) {
    std::println("{}", korka::to_string(lexed.error()));
    return 0;
  }

  auto parsed = korka::parser{lexed.value()}.parse();
  if (not parsed) {
    std::println("{}", korka::to_string(parsed.error()));
    return 0;
  }
  auto [node_pool, node_root] = parsed.value();
  std::println("{}", korka::ast_walker{node_pool, node_root, 0});

  korka::compiler compiler{node_pool, node_root};
  auto bytes = compiler.compile();

  if (bytes) {
    std::println("{}", *bytes | std::views::transform([](auto b) { return static_cast<int>(b); }));
  } else {
    std::println("{}", korka::to_string(bytes.error()));
  }
}