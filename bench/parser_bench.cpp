#include <benchmark/benchmark.h>
#include "korka/compiler/lexer.hpp"
#include "korka/compiler/parser.hpp"

static const char *simple_program =
    "int main() {\n"
    "  return 0;\n"
    "}\n";

static const char *medium_program =
    "int main() {\n"
    "  int i = 0;\n"
    "  while (i < 100) {\n"
    "    i = i + 1;\n"
    "  }\n"
    "  return i;\n"
    "}\n";

static const char *complex_program =
    "int fibonacci(int n) {\n"
    "  if (n <= 1) return n;\n"
    "  return fibonacci(n - 1) + fibonacci(n - 2);\n"
    "}\n"
    "\n"
    "int sum(int a, int b) {\n"
    "  return a + b;\n"
    "}\n"
    "\n"
    "int main() {\n"
    "  int x = 10;\n"
    "  int y = 20;\n"
    "  int z = sum(x, y);\n"
    "  int fib = fibonacci(z);\n"
    "  if (fib > 1000) {\n"
    "    return 1;\n"
    "  } else {\n"
    "    return 0;\n"
    "  }\n"
    "}\n";

static void BM_ParseSimpleProgram(benchmark::State &state) {
    auto tokens = korka::lexer{simple_program}.lex();
    for (auto _ : state) {
        korka::parser parser{tokens.value()};
        auto result = parser.parse();
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ParseSimpleProgram);

static void BM_ParseMediumProgram(benchmark::State &state) {
    auto tokens = korka::lexer{medium_program}.lex();
    for (auto _ : state) {
        korka::parser parser{tokens.value()};
        auto result = parser.parse();
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ParseMediumProgram);

static void BM_ParseComplexProgram(benchmark::State &state) {
    auto tokens = korka::lexer{complex_program}.lex();
    for (auto _ : state) {
        korka::parser parser{tokens.value()};
        auto result = parser.parse();
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_ParseComplexProgram);

static void BM_LexAndParseComplexProgram(benchmark::State &state) {
    for (auto _ : state) {
        auto tokens = korka::lexer{complex_program}.lex();
        korka::parser parser{tokens.value()};
        auto result = parser.parse();
        benchmark::DoNotOptimize(result);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_LexAndParseComplexProgram);
