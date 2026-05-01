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

These are findings from auditing the existing implementation against the
libgen guidance documents (`companion-patterns.md`,
`cmake-dependency-patterns.md`, `docs-authoring-guideline.md`,
`ecosystem-reuse.md`, `class-mapping.md`). They are recorded for triage,
not fixed in this catch-up task.

| Severity | File | Description | Recommended classification |
|---|---|---|---|
| medium | `CMakeLists.txt:41-68` | Test executable targets are named `test_<area>` rather than the libgen convention `polycpp_commander_test_<area>`. Risk: target collisions when `polycpp_commander` is consumed via `FetchContent` from another repo that also defines `test_command` or `test_option`. | bug-fix-needed |
| medium | `CMakeLists.txt` | No `POLYCPP_SOURCE_DIR` override block. Local validation against an in-tree polycpp checkout (the CI pattern) requires editing the `FetchContent_Declare(polycpp ...)` block by hand. | bug-fix-needed |
| medium | `CMakeLists.txt:31` | `option(POLYCPP_COMMANDER_BUILD_TESTS "Build tests" ON)` is unconditionally `ON`, including when consumed as a `FetchContent` subproject. Per `cmake-dependency-patterns.md` "Test wiring", default should be `ON` only when `CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR`. | bug-fix-needed |
| low | `CMakeLists.txt:38-43` | The `googletest` `FetchContent_Declare` block omits `GIT_SHALLOW TRUE`. Increases clone size and CI time. | bug-fix-needed |
| medium | `CMakeLists.txt` | `examples/greet.cpp` and `examples/todo.cpp` exist on disk but are not wired into CMake. The Sphinx pages (`docs/sphinx/examples/{greet,todo}.rst`) instruct users to run `./build/examples/greet` and `./build/examples/todo`, which will fail. Per `cmake-dependency-patterns.md`: "never let docs claim an example target exists unless CMake actually defines it." | bug-fix-needed |
| medium | `CMakeLists.txt` | No `POLYCPP_COMMANDER_BUILD_EXAMPLES` option, so the standard "build examples behind an option" pattern is missing. | bug-fix-needed |
| high | `docs/Doxyfile:43-44` | `WARN_IF_UNDOCUMENTED = NO` and `WARN_AS_ERROR = NO`. Per `docs-authoring-guideline.md` rule 12, both must be `YES`. The public-readiness gate (`scripts/check-public-readiness.py`) will reject a public release while these are `NO`. | bug-fix-needed (release blocker) |
| low | `docs/sphinx/index.rst:51` | Index card claims "600+ assertions across arguments, options, error reporting, help layout, suggestions, and subcommand dispatch." Actual ctest count is 295. The number is informational and should match reality. | bug-fix-needed |
| low | `docs/build.sh` | Retained alongside `docs/build.py`. Companions are migrating to `build.py` as the canonical entry point; keeping both invites drift. | bug-fix-needed |
| low | `include/polycpp/commander/command.hpp:1010-1036` | `Command` has many public mutable fields (`commands`, `options`, `parent`, `registeredArguments`, `args`, `rawArgs`, `processedArgs`). Upstream JS exposes them as JS object properties for compatibility with downstream code. Direct mutation could leave Command in an inconsistent state. Documented for parity, but a future revision could move these behind getters. | behavior change (already accepted for upstream parity) |
| low | `include/polycpp/commander/help.hpp` | `Help` exposes configuration as public mutable fields (`helpWidth`, `sortOptions`, etc.) rather than via a config struct. Consistent with upstream. | behavior change (already accepted for upstream parity) |
