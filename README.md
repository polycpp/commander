# polycpp-commander

C++ port of [commander.js](https://www.npmjs.com/package/commander) for [polycpp](https://github.com/enricohuang/polycpp).

A complete solution for building command-line interfaces in C++.

Port version: `0.1.0`
Initial port based on upstream version: `14.0.3`

## Status

Active. v0.1.0 implements the upstream public API for options, arguments,
subcommands, action handlers, lifecycle hooks, help formatting, "did you
mean?" suggestions, environment-variable binding, custom parsers, and
Promise-based async actions on top of polycpp's event loop. Known
divergences and deferred features are listed in
[docs/divergences.md](docs/divergences.md).

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
#include <iostream>
#include <polycpp/commander/commander.hpp>
#include <polycpp/process.hpp>

int main(int argc, char** argv) {
    polycpp::process::init(argc, argv);

    auto& prog = polycpp::commander::program();
    prog.name("myapp")
        .version("1.0.0")
        .description("My CLI application")
        .option("-v, --verbose", "enable verbose output")
        .option("-n, --name <name>", "your name");

    prog.parse();

    auto opts = prog.opts();
    const bool verbose = opts["verbose"].isBool() && opts["verbose"].asBool();
    const std::string name = opts["name"].isString() ? opts["name"].asString() : "world";
    if (verbose) std::cout << "[verbose] greeting " << name << '\n';
    std::cout << "hello, " << name << '\n';
}
```

For app-owned root commands (rather than the global singleton), use
`createCommand`, which returns a `Command` handle by value. The handle
is a thin `std::shared_ptr` wrapper around the command's state, so
copies and moves share the same underlying command:

```cpp
auto prog = polycpp::commander::createCommand("myapp");
prog.version("1.0.0").parse();
```

## Documentation

See the [docs site](docs/sphinx/index.rst) for the API reference, tutorials,
and examples. Build it locally with `python3 docs/build.py`.

Planning artifacts under `docs/`: [research.md](docs/research.md),
[dependency-analysis.md](docs/dependency-analysis.md),
[api-mapping.md](docs/api-mapping.md), [divergences.md](docs/divergences.md),
[test-plan.md](docs/test-plan.md), [port-checklist.md](docs/port-checklist.md).

## License

MIT — see [LICENSE](LICENSE) and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
