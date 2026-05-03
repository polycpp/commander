# Test Plan

## Unit tests

- `tests/test_argument.cpp` — name parsing (`<r>`, `[o]`, `<v...>`),
  required vs optional, variadic, default value with description, choices
  validation, custom argument parser, `humanReadableArgName`.
- `tests/test_option.cpp` — flag parsing, short + long combinations,
  `--no-*` negation, variadic option values, `[optional]` value, kebab to
  camelCase attribute name conversion, env-var resolution, defaults vs
  preset, choices validation, conflicts, implies, mandatory enforcement,
  hideHelp, custom parser.
- `tests/test_error.cpp` — `CommanderError(exitCode, code, message)`,
  `InvalidArgumentError` derives from `CommanderError`, exit code defaults.
- `tests/test_help.cpp` — column width calculation, sortOptions /
  sortSubcommands, showGlobalOptions, addHelpText positions
  (`before`/`after`/`beforeAll`/`afterAll`), ANSI suppression in pipes,
  `formatHelp` deterministic output, `helpInformation` returns same string
  as `outputHelp` writes, custom `Help` subclass via `createHelp()`.
- `tests/test_suggest_similar.cpp` — Damerau-Levenshtein distance,
  threshold ratio (0.4), unknown-option vs unknown-command suggestion
  formatting, single-result vs multi-result presentation.

## Integration tests

- `tests/test_command.cpp` — fluent chaining: name/description/version,
  alias / aliases, option + argument + action end-to-end, subcommand
  creation, addCommand ownership transfer, help routing, `--version`
  default option, `parse({from})` modes (`node`/`user`/`electron`),
  positional options, pass-through options, allowUnknownOption,
  allowExcessArguments, exitOverride throws instead of exiting,
  showHelpAfterError, showSuggestionAfterError.
- `tests/test_integration.cpp` — full programs assembled in a single test
  body: nested subcommands with hooks; `parseAsync()` + `actionAsync()` +
  `hookAsync()` ordering; option-source tracking
  (default → env → cli → implied); `optsWithGlobals()` merges parent
  values; `nameFromFilename` derivation.

## Compatibility tests adapted from upstream

Local C++ tests intentionally mirror the upstream Jest grouping. The
upstream suite has 113 `*.test.js` files. The local layout collapses each
upstream cluster into the matching `tests/test_<area>.cpp`:

- upstream compatibility layout: `tests/test_<area>.cpp` files (one per
  upstream cluster), with shared helpers inlined per file. A separate
  `tests/upstream/` directory was considered but rejected — it would have
  duplicated GoogleTest fixture code without improving traceability,
  because each upstream `*.test.js` already maps cleanly to a `TEST(...)`
  case inside the matching `test_<area>.cpp`.
- upstream-to-local coverage map (representative; see file headers for the
  full per-test mapping):
  - `tests/argument.*.test.js` (chain, choices, custom-processing,
    required, variadic) → `tests/test_argument.cpp`
  - `tests/options.*.test.js` (bool, bool.combo, camelcase, choices,
    conflicts, custom-processing, default, dual-options, env, flags,
    getset, implies, mandatory, optional, opts, optsWithGlobals, preset,
    registerClash, required, twice, values, variadic, version) →
    `tests/test_option.cpp`
  - `tests/option.bad-flags.test.js`, `tests/option.chain.test.js`,
    `tests/createCommand.test.js` → distributed across
    `tests/test_option.cpp` and `tests/test_command.cpp`
  - `tests/command.*.test.js` (action, addCommand, alias,
    allowExcessArguments, allowUnknownOption, argumentVariations, chain,
    configureHelp, configureOutput, copySettings, createArgument,
    createHelp, createOption, default, description, error, exitOverride,
    helpCommand, helpOption, hook, name, nested, onCommand,
    option-misuse, parse, parseOptions, positionalOptions,
    registerClash, showHelpAfterError, showSuggestionAfterError, summary,
    unknownCommand, unknownOption, usage) → `tests/test_command.cpp`
  - `tests/command.commandHelp.test.js`, `tests/command.help.test.js`,
    `tests/command.addHelpOption.test.js`, `tests/command.addHelpText.test.js`,
    `tests/help.*.test.js`, `tests/helpGroup.test.js`,
    `tests/useColor.test.js` → `tests/test_help.cpp`
  - `tests/help.suggestion.test.js`, plus the suggestion paths inside
    `tests/command.unknownOption.test.js` / `unknownCommand.test.js` →
    `tests/test_suggest_similar.cpp`
  - `tests/program.test.js`, `tests/commander.configureCommand.test.js`,
    `tests/negatives.test.js`, `tests/command.asterisk.test.js` →
    `tests/test_integration.cpp`
  - `tests/command.executableSubcommand.test.js`,
    `tests/command.executableSubcommand.lookup.test.js`,
    `tests/command.executableSubcommand.search.test.js`,
    `tests/command.executableSubcommand.mock.test.js`,
    `tests/command.executableSubcommand.signals.test.js` →
    `tests/test_executable.cpp`. The executable test target compiles
    `echo_argv` and `exit_with_code` on every platform, and adds
    `signal_writer` on POSIX platforms where signal forwarding is
    available. Fixtures are located via CMake target file paths rather
    than hard-coded binary-directory names, so the same tests work with
    single-config and multi-config generators. Covered scenarios: default
    `<prog>-<sub>` name resolution; explicit `executableFile` (relative,
    absolute, and `./bin/...` path-containing forms); `executableDir`
    precedence over `PATH`; `executableDir` joined relative to
    `scriptPath` (node-mode parse); fall-back PATH search; Windows `.exe`
    candidate expansion; Windows `.cmd`/`.bat` shim lookup through
    `COMSPEC`/`cmd.exe`; semicolon-delimited Windows `PATH` search;
    missing-executable `commander.executeSubCommandAsync` error;
    operands and unknown args forwarded to child in the documented
    order; `--` separator behaviour; non-zero/zero child exit
    propagation; non-executable candidates skipped during search;
    fluent-chain return-`*this` semantics; `executableHandler_` flag
    flipping behaviour-verified via the spawn dispatch path; mandatory
    + conflicting option checks firing **before** spawn;
    `addCommand(...)` of an executable-flagged sub still routing to the
    spawn path; `parseAsync()` reaching the same dispatch path; help
    output listing executable subcommands; Windows `.cmd` shim dispatch
    through `COMSPEC`/`cmd.exe`; POSIX-only signal forwarding for
    SIGTERM, SIGINT, SIGHUP, SIGUSR1, and SIGUSR2 (parent → child via
    `ChildProcess::kill(signum)`); forwarder is inactive after the
    child exits; multiple sequential spawns each install their own
    forwarder; user-installed `process::on(SIG, ...)` handlers coexist
    with the per-spawn forwarder; child exit code on signal
    (`128 + signum`) surfaces through `exitOverride()` as a
    `CommanderError`.
- omitted upstream cases:
  - `tests/command.executableSubcommand.inspect.test.js` — omitted;
    covers `node --inspect` argv splitting (Node-runtime specific) which
    is also deferred.
  - `tests/incrementNodeInspectorPort.test.js` — omitted; covers
    `node --inspect` argv-rewriting which is deferred.
  - `tests/deprecated.test.js` — omitted; covers JS-only deprecation
    warning hooks that have no C++ counterpart.
  - `tests/esm-imports-test.mjs`, `tests/ts-imports.test.ts`,
    `tests/fixtures-extensions/*` — omitted; cover Node module-loader
    behavior (CJS/ESM/TS interop) that is not meaningful for a C++
    library.
  - `tests/help.preformatted.test.js`, `tests/help.boxWrap.test.js` —
    partial: long-line wrapping is exercised by the local
    `test_help.cpp`. The full upstream Jest snapshot fixture is not
    transcribed verbatim because the local tests assert the same width
    invariants directly.

## Security and fail-closed tests

- unknown option / unknown command without `allowUnknownOption(true)` →
  `CommanderError` (covered in `tests/test_command.cpp`).
- excess arguments without `allowExcessArguments(true)` → `CommanderError`
  (covered in `tests/test_command.cpp`).
- mandatory option missing → `CommanderError` (covered in
  `tests/test_option.cpp`).
- choices validation rejects out-of-set values for both options and
  arguments → `InvalidArgumentError` (covered in `tests/test_option.cpp`,
  `tests/test_argument.cpp`).
- `exitOverride()` throws `CommanderError` instead of calling
  `process::exit`, allowing host applications to catch parse failures
  (covered in `tests/test_command.cpp`).
- suggestion output does not echo arbitrary user input back without
  sanitization (covered in `tests/test_suggest_similar.cpp`).
- `executableDir` lookup precedence and executable-subcommand path search
  are bounded to regular files, with POSIX executable-bit checks and
  Windows extension/PATHEXT candidate checks (covered in
  `tests/test_executable.cpp`).

## Protocol/client tests

- not applicable because commander is a CLI argument parser and has no
  network, database, or wire-protocol surface.

## Release-blocking behaviors

- camelCase attribute name conversion is correct for every fixture in
  `options.camelcase.test.js`.
- `--no-*` negation clears or inverts the boolean as upstream does.
- option-source ordering (`default` → `config` → `env` → `cli` →
  `implied`) gives the same winner as upstream.
- help output is deterministic (same input → same string) so downstream
  golden-file tests work.
- `parseAsync()` resolves only after all action handlers and hooks
  (sync-wrapped or genuinely async) have completed.
- `exitOverride()` works — host applications must be able to embed
  commander without losing control flow.

## Current validation

- POSIX build commands:
  - `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug`
  - `cmake --build build -j$(nproc)`
- POSIX test command:
  - `cd build && ctest --output-on-failure`
- POSIX environment:
  - GCC 13.x or Clang 16+, CMake 3.20+, Ninja
  - `polycpp` master pulled via FetchContent (CMake `FetchContent_Declare`
    targets `git@github.com:enricohuang/polycpp.git`)
  - GoogleTest v1.15.2 pulled via FetchContent
- Windows/MSVC build commands:
  - `$polycpp = (Resolve-Path ..\polycpp).Path`
  - `cmake -S . -B build-win -DPOLYCPP_SOURCE_DIR="$polycpp" -DPOLYCPP_SSL_BACKEND=boringssl -DOPENSSL_NO_ASM=ON`
  - `cmake --build build-win --config Debug --parallel`
- Windows/MSVC test command:
  - `ctest --test-dir build-win -C Debug --output-on-failure`
- Windows/MSVC environment:
  - Visual Studio Community 2022 detected by `vswhere` with MSVC
    19.44.35211, CMake 3.31.11, Visual Studio multi-config generator
  - local `polycpp` checkout supplied with `POLYCPP_SOURCE_DIR`
  - BoringSSL selected because OpenSSL was not installed; `OPENSSL_NO_ASM`
    was required on this host to avoid a missing generated assembly object
    during the BoringSSL build
  - 324/324 CTest-discovered tests passed on Windows/MSVC
- last validated against polycpp HEAD `4ed62f96…` (2026-05-01); see
  `docs/research.md` for the full snapshot.
