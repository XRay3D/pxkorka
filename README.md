<img width="64" src="./icon.svg" align="left" alt="KorkaVM icon">

# KorkaVM

A Virtual Machine where lexing and compilation happen entirely at **compile-time**.

---

### What is this

KorkaVM is a project where I'm trying to create a tool that allows to embed logic
without runtime overhead of parsing or loading external files. You write C-like code
right inside C++, and the compiler transforms it into internal bytecode before your
program even starts.

### Status

|    Component     |  Stage  | Execution context |
|:----------------:|:-------:|:-----------------:|
|      Lexer       |  Done   |     constexpr     |
| Bytecode builder |  Done   |     constexpr     |
|      Parser      |  Done   |     constexpr     |
|     Compiler     |   Partially done   |     constexpr     |
|    VM runner     | WIP |      runtime      |

What's done:
```cpp
constexpr char code[] = R"(
int main() {
  int a = 2;
  if (a) {
    return a;
  } else {
    return 5 + a;
  }
}

int foo(int a, int b) {
  return a + b;
}
)";

constexpr auto compile_result = korka::compile<code>();

// Extracting function types from code
// It returns a pointer, bc you can't return a type ._.
auto main_func = compile_result.function<"main">();
static_assert(std::is_same_v<decltype(main_func), long (*)()>);

auto foo_func = compile_result.function<"foo">();
static_assert(std::is_same_v<decltype(foo_func), long (*)(long, long)>);
```


## Example context

```cpp

auto foo(int a) -> int {
  return a * 2 + 5;
}

constexpr auto bindings = korka::make_bindings(
  "foo", &foo
);

constexpr auto my_script = korka::compile(bindings, R"(
    int calculate(int x) {
        if (x <= 0) return 0;

        return foo(x) / 3;
    }
)");

// Simple usage
int main() {
  korka::runtime vm;
  int result = vm.execute(my_script)
}

// Not so simple usage
int main() {
  korka::runtime vm;
  
  // Byte code gets inserted right into the native instruction flow
  // and executed by vm right there
  korka::run_embed<my_script>(vm);
}
```
