# polycpp-commander

C++ port of [commander.js](https://www.npmjs.com/package/commander) for [polycpp](https://github.com/enricohuang/polycpp).

A complete solution for building command-line interfaces in C++.

## Prerequisites

- C++20 compiler (GCC 13+ or Clang 16+)
- CMake 3.20+
- Ninja (recommended)

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

## Usage

```cpp
#include <polycpp/commander/detail/aggregator.hpp>
#include <iostream>

int main(int argc, char** argv) {
    polycpp::process::init(argc, argv);

    auto program = polycpp::commander::createCommand("myapp");
    program.version("1.0.0")
        .description("My CLI application")
        .option("-v, --verbose", "enable verbose output")
        .option("-n, --name <name>", "your name");

    program.parse();

    auto opts = program.opts();
    // Use opts...
}
```

## License

MIT — see [LICENSE](LICENSE).
