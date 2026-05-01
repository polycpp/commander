# Divergences From Upstream

## Deferred Features

- **Windows-specific stand-alone executable lookup.** Upstream resolves
  `<prog>-<sub>` against `.cmd` and `.bat` shims, and rewrites
  `node --inspect` flags to allocate distinct ports for parent and child.
  The C++ port currently performs POSIX-style search only. Re-enable when a
  Windows tester is available; the search loop in
  `Command::executeSubCommand_` is the only place that needs to grow the
  Windows extension list.
- **Electron auto-detection of `argv[0]`.** Upstream inspects
  `process.defaultApp` to decide whether to skip the first element of
  `argv`. The port replaces this with an explicit
  `parse(argv, {.from = "electron"})` opt-in. Auto-detection can be added
  later by reading `polycpp::process::env("ELECTRON_RUN_AS_NODE")`.

## Deliberate Behavior Changes

- **`Command` is non-copyable, owned by `unique_ptr`.** `polycpp::events::EventEmitter`
  is non-copyable, so `Command` is non-copyable too. Subcommands are stored
  as `std::vector<std::unique_ptr<Command>>`; `addCommand`,
  `addHelpCommand`, and `createCommand` all use `unique_ptr`. In JS the
  same shape is reference semantics over a JS object — the user-visible
  API still chains identically: `prog.command("serve").action(...)`.
- **Sync vs. async action and hook overloads are separate methods.**
  Upstream collapses sync and async into a single `action(fn)` /
  `hook(event, fn)` and dispatches by the runtime type of the returned
  value. C++ instead exposes `action()` / `actionAsync()` and `hook()` /
  `hookAsync()`. This makes the typing explicit, avoids ambiguous overloads
  on `std::function`, and matches what the upstream TypeScript declarations
  already document at the type level.
- **`Option#default` / `Argument#default` renamed to `defaultValue`.**
  `default` is a C++ keyword and cannot be used as a method name. The
  semantics are identical.
- **`Command#command(spec, description, opts)` (executable form) is a
  separate method `executableCommand(spec, description, executableFile?)`.**
  Upstream uses overload resolution on the second argument's type to pick
  between the inline-action form and the stand-alone executable form. C++
  cannot disambiguate by string vs. options struct cleanly, so the
  executable form is its own named method.
- **`Error.captureStackTrace` is not preserved.** It is a Node-only
  observability hook with no C++ equivalent.
- **`createCommand` returns `std::unique_ptr<Command>`.** Same reason as
  the non-copyable note above. JS callers get a fresh `Command` object;
  C++ callers get owning storage they can move into `addCommand` or use
  directly via `*ptr`.

## Unsupported Runtime-Specific Features

- **JavaScript prototype mutation hooks** (e.g., subclassing by reassigning
  `Command.prototype.foo`) are not meaningful in C++. The C++ port instead
  exposes `virtual` factory methods (`Command::createCommand`,
  `Command::createOption`, `Command::createArgument`, `Command::createHelp`)
  for subclass customization.
- **Node-specific globals** (`global`, `globalThis`, `require.main`) are
  not exposed.
- **JS-style dynamic `command:*` event listeners** are partially adapted:
  the named lifecycle events (`preSubcommand`, `preAction`, `postAction`)
  and `option:<flag>` / `optionEnv:<flag>` events are emitted through
  `polycpp::events::EventEmitter`. Wildcard `command:*` listening (which in
  JS relies on EventEmitter wildcard plugins) is not preserved; users
  should attach a `Command::hook("preSubcommand", ...)` instead.

## Audit findings (libgen catch-up)

Findings from auditing the existing implementation against the libgen
guidance documents (`companion-patterns.md`,
`cmake-dependency-patterns.md`, `docs-authoring-guideline.md`,
`ecosystem-reuse.md`, `class-mapping.md`).

### Resolved

| Severity | File | Description | Resolution |
|---|---|---|---|
| high | `docs/Doxyfile:43-44` | `WARN_IF_UNDOCUMENTED = NO`, `WARN_AS_ERROR = NO`. Per `docs-authoring-guideline.md` rule 12, both must be `YES`. Public-readiness gate blocker. | Set both to `YES`; added `DOXYGEN=1` to `PREDEFINED`. Docs build still passes. |
| high | `include/polycpp/commander/detail/command.hpp:485-491` | `Command::parse()` (no-arg) was wired to `parse({}, {.from = "node"})` instead of `process::argv()`. The intent comment said "use process::argv()" but the implementation was a placeholder. Every README/quickstart example using `prog.parse()` would silently see an empty argv and report missing required args. | `parse()` now reads `process::argv()` and reshapes the native `[program, ...userArgs]` to Node-style `[runtime, script, ...userArgs]` so the existing "node" mode strips the right number of leading entries. Examples now run. |
| medium | `CMakeLists.txt:41-68` | Test executable targets named `test_<area>` instead of `polycpp_commander_test_<area>`. Subproject collision risk. | Renamed via foreach loop; the seven test executables are now `polycpp_commander_test_{error,argument,option,suggest_similar,help,command,integration}`. |
| medium | `CMakeLists.txt` | No `POLYCPP_SOURCE_DIR` override. | Added the standard libgen baseline block: local checkout via `POLYCPP_SOURCE_DIR`, fall back to GitHub master. |
| medium | `CMakeLists.txt:31` | `POLYCPP_COMMANDER_BUILD_TESTS` defaulted ON unconditionally. | Default is now ON only when `CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR`. |
| medium | `CMakeLists.txt` | Examples not wired into CMake; docs instructed running `./build/examples/{greet,todo}`. | Added `POLYCPP_COMMANDER_BUILD_EXAMPLES` option (same default-on-standalone semantics) plus a glob-driven loop that builds `examples/*.cpp` into `build/examples/<name>`. |
| medium | `CMakeLists.txt` | No `POLYCPP_COMMANDER_BUILD_EXAMPLES`. | Added with the same default-on-standalone semantics as the test option. |
| low | `CMakeLists.txt:38-43` | `googletest` fetch missed `GIT_SHALLOW TRUE`. | Added; also switched the fetch URL to `https://github.com/...` because SSH access fails in some CI/build environments. |
| low | `docs/sphinx/index.rst:51` | Claimed "600+ assertions"; actual is 295. | Updated to "295 GoogleTest cases". |
| low | `docs/build.sh` | Retained alongside `docs/build.py`. | Removed; `docs/build.py` is the only entry point. |
| low | `examples/greet.cpp:35` | Used `opts["shout"].asBool()` without checking the option was set; threw `polycpp::TypeError` for a null value. | Now guards with `isBool()` first. |
| low | `examples/todo.cpp:64` | Same pattern for `--verbose`. | Same fix. |
| low | `examples/todo.cpp:58-62` | `--priority <n>` had no parser, so `opts["priority"].asInt()` threw because the CLI value was a string. | Added an `argParser` that converts the string to int via `std::stoi`. |
| low | `examples/todo.cpp:85` | `args[0].asInt()` on a string-typed positional argument crashed. | Now uses `std::stoi(args[0].asString())`. |
| low | `README.md` Usage example | Same pattern as greet: `opts["verbose"].asBool()` with no guard, would crash when `--verbose` was absent. | Updated the README example to use `isBool()` / `isString()` guards and provide a default value for `name`. |
| low | `docs/sphinx/examples/index.rst:22-23` | Used `<example_name>` placeholder text that the public-readiness regex flags as a generated placeholder. | Replaced with a concrete example using `polycpp_commander_example_greet`. |
| low | `docs/sphinx/tutorials/custom-help.rst:54` | The string ending the word "Environmen" with a colon followed by an escape character matched the libgen public-path scanner's Windows-drive regex (letter+colon+backslash). | Removed the colon from the help-text label. |

### Remaining (intentional, parity-driven)

| Severity | File | Description | Classification |
|---|---|---|---|
| low | `include/polycpp/commander/command.hpp:1010-1036` | `Command` has many public mutable fields (`commands`, `options`, `parent`, `registeredArguments`, `args`, `rawArgs`, `processedArgs`). Mirrors upstream's JS object property shape. Direct mutation could leave a `Command` in an inconsistent state. | behavior change (accepted for upstream parity) |
| low | `include/polycpp/commander/help.hpp` | `Help` exposes configuration as public mutable fields (`helpWidth`, `sortOptions`, etc.) rather than via a config struct. Mirrors upstream. | behavior change (accepted for upstream parity) |
