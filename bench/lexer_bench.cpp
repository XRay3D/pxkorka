#include <benchmark/benchmark.h>
#include "korka/compiler/lexer.hpp"

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

static void BM_LexSimpleProgram(benchmark::State &state) {
    for (auto _ : state) {
        korka::lexer lexer{simple_program};
        auto tokens = lexer.lex();
        benchmark::DoNotOptimize(tokens);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_LexSimpleProgram);

static void BM_LexMediumProgram(benchmark::State &state) {
    for (auto _ : state) {
        korka::lexer lexer{medium_program};
        auto tokens = lexer.lex();
        benchmark::DoNotOptimize(tokens);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_LexMediumProgram);

static void BM_LexComplexProgram(benchmark::State &state) {
    for (auto _ : state) {
        korka::lexer lexer{complex_program};
        auto tokens = lexer.lex();
        benchmark::DoNotOptimize(tokens);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_LexComplexProgram);

BENCHMARK_MAIN();
