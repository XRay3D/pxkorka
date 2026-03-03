#include <benchmark/benchmark.h>
#include "korka/vm/bytecode_builder.hpp"
#include "korka/vm/op_codes.hpp"
#include "korka/vm/options.hpp"

static void BM_EmitArithmeticOps(benchmark::State &state) {
    for (auto _ : state) {
        korka::vm::bytecode_builder b{};
        b.emit_add(0, 1, 2);
        b.emit_sub(3, 4, 5);
        b.emit_mul(6, 7, 8);
        b.emit_div(9, 10, 11);
        auto bytes = b.build();
        benchmark::DoNotOptimize(bytes);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_EmitArithmeticOps);

static void BM_EmitJumps(benchmark::State &state) {
    for (auto _ : state) {
        korka::vm::bytecode_builder b{};
        auto target = b.make_label();
        b.emit_jmp(target);
        b.emit_add(0, 1, 2);
        b.bind(target);
        b.emit_add(3, 4, 5);
        auto bytes = b.build();
        benchmark::DoNotOptimize(bytes);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_EmitJumps);

static void BM_BuildComplexBytecode(benchmark::State &state) {
    for (auto _ : state) {
        korka::vm::bytecode_builder b{};

        auto loop_start = b.make_label();
        auto loop_end = b.make_label();

        b.emit_load_imm(0, korka::vm::stack_value_t{0});
        b.emit_load_imm(1, korka::vm::stack_value_t{100});
        b.emit_load_imm(2, korka::vm::stack_value_t{1});

        b.bind(loop_start);
        b.emit_cmp_lt(3, 0, 1);
        b.emit_jmp_if(loop_end, 3);
        b.emit_add(0, 0, 2);
        b.emit_jmp(loop_start);

        b.bind(loop_end);
        auto bytes = b.build();
        benchmark::DoNotOptimize(bytes);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_BuildComplexBytecode);
