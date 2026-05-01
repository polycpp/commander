# Research

- package: commander
- npm url: https://www.npmjs.com/package/commander
- source url: https://github.com/tj/commander.js.git
- upstream version basis: 14.0.3
- upstream revision analyzed: 8247364da749736570161e95682b07fc2d72497b
- upstream default branch: master
- license: MIT
- license evidence: package.json license field, manual SPDX check: upstream LICENSE confirms MIT
- category: CLI / argument parser
- port version: 0.1.0
- versioning note: port version is independent from upstream versioning; the C++ port owns its SemVer

## Package purpose

`commander` is the de-facto Node.js library for declaring CLI programs: it
parses `argv`, declares options/arguments/subcommands with a fluent builder,
binds env vars, dispatches action handlers, formats `--help`, suggests close
matches on typos, and supports both inline and stand-alone (`<prog>-<sub>`)
executable subcommands.

The C++ port preserves the same fluent surface so that a developer with
commander.js experience can write `program.option(...).argument(...).action(...).parse()`
unchanged in shape.

## Runtime assumptions

- browser: no — upstream is Node.js only (uses `process`, `fs`, `child_process`, `path`)
- node.js: yes — entry `index.js` re-exports from `lib/`
- filesystem: read-only — used for executable subcommand search (`fs.existsSync`, `fs.realpathSync`) and to derive the program name from `argv[1]`
- network: none
- crypto: none
- terminal: stdout/stderr only, with `isTTY`, `columns`, and `hasColors` detection for help formatting
- subprocess: yes — `child_process.spawn` is used for stand-alone executable subcommands, and signal forwarding (`SIGUSR1`/`SIGUSR2`/`SIGTERM`/`SIGHUP`/`SIGINT`) is wired to the spawned child

## Dependency summary

- package.json present: yes
- package main: ./index.js
- package types: typings/index.d.ts
- package exports: ., ./esm.mjs
- package bin: none
- hard dependencies: none — commander is dependency-free
- peer dependencies: none
- optional dependencies: none
- dependency analysis report: `docs/dependency-analysis.md`

## Upstream repo layout summary

Clone path used for analysis: `<repo path>/.tmp/upstream/commander.js`
Published npm artifact: `<repo path>/.tmp/npm-package` (commander 14.0.3)

Top-level layout:

- `index.js`, `esm.mjs` — re-export the `lib/` modules (both CJS and ESM entry points)
- `lib/command.js` — the `Command` class (the core)
- `lib/option.js` — the `Option` class
- `lib/argument.js` — the `Argument` class
- `lib/help.js` — the `Help` class (formatting and layout)
- `lib/error.js` — `CommanderError`, `InvalidArgumentError`
- `lib/suggestSimilar.js` — Damerau-Levenshtein "did you mean?" suggestions
- `typings/index.d.ts` — TypeScript public-API contract
- `tests/*.test.js` — 113 Jest test files, one per feature cluster
- `examples/` — runnable demos, useful as porting fixtures
- `docs/` — narrative deep-dive docs for help, options, parsing, hooks, terminology

## Entry points used by consumers

- `require('commander')` → `index.js` → `lib/command.js`
- `require('commander').program` — global Command singleton
- `require('commander').Command` — class for app-owned root commands
- `require('commander').Option`, `Argument`, `Help`, `CommanderError`, `InvalidArgumentError`
- `import { program, Command, ... } from 'commander'` (ESM via `esm.mjs`)

TypeScript declarations inspected: `typings/index.d.ts` defined the public
overload shapes for `option()`, `requiredOption()`, `command()`, `parse()`,
`parseAsync()`, action callbacks (sync/async), hooks (sync/async), and the
dual getter/setter pattern for metadata accessors. The C++ port mirrors that
contract.

## Important files and why they matter

- `lib/command.js` (~2 kLoC) — the parsing state machine, hook lifecycle,
  subcommand dispatch, env-var resolution, conflict/required/excess checks,
  help routing, exit override, and stand-alone executable spawning. This is
  the single most important porting target and the largest behavior surface.
- `lib/option.js` — flag parsing (`-f, --flag <value>`), `--no-*` negation,
  variadic, kebab→camel attribute names, default/preset/env/choices/conflicts/implies.
- `lib/argument.js` — `<required>`, `[optional]`, `<variadic...>` parsing,
  default/choices/custom-parser.
- `lib/help.js` — column-aware help layout, ANSI color suppression in pipes,
  sortOptions/sortSubcommands/showGlobalOptions, addHelpText positions.
- `lib/error.js` — `CommanderError(exitCode, code, message)` and the
  `InvalidArgumentError` subclass used by custom parsers/choices.
- `lib/suggestSimilar.js` — used after unknown-option/unknown-command errors.
- `typings/index.d.ts` — overload shapes for sync/async actions, hooks, and
  parse variants; informs the C++ overload split.

## Files likely irrelevant to the C++ port

- `eslint.config.js`, `.prettierrc.js`, `.editorconfig` — JS-only tooling
- `tsconfig.*.json`, `jest.config.js` — build/test wiring for the JS project
- `package-support.json`, `SECURITY.md`, `CHANGELOG.md` — release metadata
- `Readme_zh-CN.md` — translated README
- `esm.mjs` — ESM bridge; functionally identical surface to `index.js`

## Test directories worth mining first

- `tests/*.test.js` (113 files) — each file targets a single feature cluster
  (option, argument, command, help, hook, error, executable subcommand,
  suggestion). The local C++ tests intentionally mirror this grouping under
  `tests/test_<area>.cpp`.
- `tests/fixtures/` — runnable scripts used by executable-subcommand tests;
  ported as inline fixtures or skipped where they require Node.

## Implementation risks discovered from the source layout

- **Help layout fidelity**: column wrapping, indentation, sort ordering, and
  ANSI color escapes interact subtly. Help text is the primary user-visible
  output, so even a one-character drift is a regression.
- **Option-value source tracking**: `default` < `config` < `env` < `cli` <
  `implied` ordering controls which value wins; commander.js threads this
  through `setOptionValueWithSource`. The port mirrors the same source string
  set.
- **Executable subcommand spawn**: relies on `child_process.spawn`,
  signal forwarding, and `node --inspect` argv splitting. The port maps to
  `polycpp::child_process::spawn` and re-forwards POSIX signals; Windows-only
  inspect-flag rewriting is not in scope for v0.
- **Promise-based async parse**: `parseAsync()` must wait for both
  user-provided async actions and async hooks before resolving — a subtle
  ordering surface.
- **camelCase attribute names**: `--dry-run` → `opts["dryRun"]`. Easy to get
  wrong in edge cases (`--no-foo`, single-dash, multiple dashes).
- **`--no-*` boolean negation**: must clear the value or set it to `false`
  depending on whether a default was set.
- **`combineFlagAndOptionalValue`**, **`passThroughOptions`**,
  **`enablePositionalOptions`**, **`allowUnknownOption`**,
  **`allowExcessArguments`** — five orthogonal parser-mode flags whose
  interaction is the part most heavily exercised by upstream tests.

## Companion repo alignment

Reference companions inspected at `<companion libs root>`:

- small utility: `cookie`, `mime`, `vary` — same `include/polycpp/<name>/`
  layout, single-TU `src/<name>.cpp`, `polycpp::<name>` CMake alias,
  `detail/aggregator.hpp` umbrella, Sphinx + Doxygen docs scaffold.
- API-rich: `yaml`, `winston`, `moment`, `express` — inline-implementation
  pattern with declarations in `include/polycpp/<name>/*.hpp` and
  inline impls in `include/polycpp/<name>/detail/*.hpp`, all exposed through
  `detail/aggregator.hpp`.

Decisions:

- CMake target name: `polycpp_commander`; alias `polycpp::commander` — matches
  the convention used by every existing companion.
- Normalized namespace: `polycpp::commander`; include directory
  `include/polycpp/commander/`.
- Public headers vs `detail/`: declarations in `include/polycpp/commander/*.hpp`,
  inline implementations in `include/polycpp/commander/detail/*.hpp`,
  re-exported from `detail/aggregator.hpp` so users can `#include
  <polycpp/commander/commander.hpp>` for the public entry point only.
- A `detail/aggregator.hpp` header is present and required to manage the
  declaration-then-implementation include order (`Command` and `Help` are
  mutually recursive).
- Public API naming style: camelCase functions/methods/option fields
  (`createCommand`, `parseAsync`, `helpInformation`, `outputConfiguration`),
  matching upstream commander.js and matching every existing polycpp
  companion. snake_case is not exposed.
- Examples strategy: real runnable `examples/*.cpp` (`greet.cpp`, `todo.cpp`)
  built into separate executables, mirroring `cookie`/`yaml`.
- Docs site strategy: Doxygen → Breathe → Sphinx, identical scaffold to other
  companions, hosted via the standard `.github/workflows/docs.yml`.
- README structure: install via FetchContent, prerequisites, build, basic
  usage, link to the docs site. Same skeleton as the other companions.
- Deliberate deviations from existing companions: none structural after
  the v0.2 Handle/Impl refactor. `Command` is now a thin handle that
  inherits from `polycpp::events::EventEmitterForwarder` (which forwards
  typed-event operations to an internal `polycpp::events::EventEmitter`),
  matching commander.js's `Command extends EventEmitter` design while
  keeping JS-style reference semantics. Subcommands are stored as
  `std::vector<Command>` (handles, not `unique_ptr`). The earlier
  `unique_ptr`-based ownership was the only structural divergence and has
  been resolved — see `docs/divergences.md` ▸ "Resolved (post-catch-up)".

## Polycpp ecosystem reuse analysis

- polycpp core paths inspected: `<polycpp checkout>/include/polycpp/{process,events,core,child_process,fs,path}`
- polycpp capability snapshot: `git rev-parse HEAD` =
  `4ed62f962795de4de6d8d049d52561b2d712b2c0`, checked 2026-05-01.
- transport/listener capability review: not applicable — commander has no
  network, socket, listener, or TLS surface. The package is offline.
- polycpp core types/functions selected: `polycpp::process` (argv/exit/env/init), `polycpp::events::EventEmitter`, `polycpp::JsonValue`/`JsonObject`, `polycpp::Promise<T>`, `polycpp::Error`, `polycpp::child_process::spawn`, `polycpp::EventLoop`. Details:
  - `polycpp::process::init(argc, argv)` — required by examples to seed argv
  - `polycpp::process::argv()` — default source for `parse()`
  - `polycpp::process::env(name)` — backs `Option.env(name)`; the recent flat
    string-map shape is the one consumed (see commit 2dc8401)
  - `polycpp::process::exit(code)` — backs the default exit path
  - `polycpp::events::EventEmitter` — owned by each `Command::Impl` and
    forwarded to via `polycpp::events::EventEmitterForwarder` on the
    public `Command` handle; underpins the `option:flag` /
    `optionEnv:flag` / hook event fan-out
  - `polycpp::JsonValue`, `polycpp::JsonObject` — public option/argument
    value type, mirrors the dynamic JS value
  - `polycpp::Promise<T>` — return type for `parseAsync()` and the
    `actionAsync()` / `hookAsync()` overloads
  - `polycpp::Error` — base class for `CommanderError`
  - `polycpp::child_process::spawn` — backs stand-alone executable subcommand
    dispatch
  - `polycpp::EventLoop` — driven by example code that runs `parseAsync()`
- polycpp core types/functions rejected: `polycpp::String`, `polycpp::Buffer`, and the `polycpp::http`/`polycpp::tls`/`polycpp::net` modules. Details:
  - `polycpp::String` — rejected as the public string type. commander does not
    expose JavaScript UTF-16 code-unit semantics; option names, descriptions,
    and help text are plain UTF-8 ASCII-friendly text. `std::string` is
    correct.
  - `polycpp::Buffer` — not used. Upstream only references `Buffer.isBuffer`
    in a single defensive check that is not part of the public API.
  - `polycpp::http::*`, `polycpp::tls::*`, `polycpp::net::*` — not applicable.
- public polycpp interop review:
  - public text APIs use `std::string` (UTF-8). Recorded.
  - object-shaped APIs accept and return `polycpp::JsonValue` (option values,
    argument values). `OptionValues` is `polycpp::JsonValue` (an object).
  - no Date/time API surface. `polycpp::Date` is not relevant.
  - diagnostic/config structs: `OutputConfiguration`, `ParseOptions`,
    `ParseOptionsResult`, `HelpContext`, `ErrorOptions`, `CommandOptions`,
    `PrepareContextOptions` are deliberately typed C++ structs rather than
    `JsonValue` blobs because their shape is fixed. They do not need
    `toObject()`/`toJSON()` adapters for v0 — the only one a user might want
    to serialize is `helpConfiguration_`, and that is already a
    `std::map<std::string, JsonValue>`.
  - `polycpp::JSON::stringify(opts)` works directly on the value returned by
    `Command::opts()` because it is already a `JsonValue`. No extra adapter
    needed.
- companion libs inspected for reusable APIs: `cookie`, `mime`, `vary`,
  `yaml`, `winston`, `moment`, `express`. None of them export an
  argument-parsing or CLI-help surface. `commander` is itself a candidate to
  be consumed by other companions (e.g., a CLI on top of `winston` could use
  it).
- companion libs selected for reuse: none — commander has no runtime
  dependencies in JS and none in C++.
- companion libs rejected or deferred: none.
- new local abstractions introduced: `Command`, `Option`, `Argument`, `Help`,
  `CommanderError`, `InvalidArgumentError`, `OutputConfiguration`,
  `ParseOptions`, `ParseOptionsResult`, `HelpContext`, `ErrorOptions`,
  `CommandOptions`, `PrepareContextOptions`, `SplitFlags`, free functions
  `program()`, `createCommand()`, `createOption()`, `createArgument()`,
  `camelcase()`, `splitOptionFlags()`, `humanReadableArgName()`,
  `suggestSimilar()`. Every one of these directly mirrors a commander.js
  symbol; none reimplements a primitive that exists in base polycpp or in
  another companion.
- reuse risks or integration gaps: `polycpp::process::env()` shape evolved (already adapted in commit 2dc8401); Windows-specific `node --inspect` argv rewriting is not provided by `polycpp::child_process::spawn`. Details:
  - `polycpp::events::EventEmitter` is non-copyable, but `Command` no
    longer extends it directly: the v0.2 refactor moved the emitter into
    a `Command::Impl` and made `Command` a thin handle that inherits from
    `polycpp::events::EventEmitterForwarder`. Copies of the handle share
    the same Impl, mirroring the JS reference-semantics model.
    Subcommands are now stored as `std::vector<Command>` (handles by
    value). The unique_ptr-based ownership has been removed.
  - `polycpp::process::env()` shape changed (commit 2dc8401 adapted to flat
    string map). The implementation already tracks this; no further gap.
  - `polycpp::child_process::spawn` does not implement Windows-specific
    `node --inspect` argv rewriting. Out of scope for v0; recorded in
    divergences.

## Node parity surface audit

- callback APIs: action handlers (`ActionFn`) and hooks (`HookFn`,
  preSubcommand/preAction/postAction) are `std::function`. These are direct
  C++ callback parity for upstream's `(args, opts, command) => {}` and
  `(thisCommand, actionCommand) => {}` shapes.
- Promise APIs: `parseAsync()`, `actionAsync()`, `hookAsync()` map to
  `polycpp::Promise<void>` / `polycpp::Promise<std::reference_wrapper<Command>>`.
  Sync handlers are wrapped in immediately-resolving promises so a caller can
  mix sync and async without shape changes.
- EventEmitter APIs: `Command` extends `polycpp::events::EventEmitter`.
  Internal events `option:<flag>`, `optionEnv:<flag>`, and the hook
  lifecycle events (`preSubcommand`, `preAction`, `postAction`) are emitted
  via `EventEmitter`. The public surface for user-attached listeners is
  preserved (`prog.on("option:verbose", ...)`).
- server/listener APIs: not applicable — commander has no network surface.
- diagnostic/tracing APIs: not applicable — commander does not emit
  diagnostics_channel events.
- stream APIs: not applicable — commander writes lines via
  `process.stdout.write` / `process.stderr.write`. The C++ port routes
  through `OutputConfiguration::writeOut` / `writeErr`, which default to
  `std::cout`/`std::cerr`.
- Buffer and binary APIs: not applicable — the only `Buffer` reference in
  upstream is `Buffer.isBuffer` in a defensive type check; no binary I/O.
- URL, timer, process, and filesystem APIs: backed by `polycpp::process` (argv, env, exit, signals), `polycpp::path`, and `polycpp::fs` / `<filesystem>`. Timers and URLs are not used. Details:
  - `process.argv`, `process.argv[0]`, `process.execArgv`, `process.execPath`,
    `process.platform`, `process.defaultApp`, `process.exit`, `process.exitCode`,
    `process.on('SIGINT')` — backed by `polycpp::process`.
  - `process.env.NO_COLOR`, `FORCE_COLOR`, `CLICOLOR_FORCE` — read via
    `polycpp::process::env()` for color suppression.
  - `process.stdout.isTTY`, `columns`, `hasColors`; same for stderr — backed by
    a defaulted `OutputConfiguration` callback set; users can override.
  - `fs.existsSync`, `fs.realpathSync` — used for executable-subcommand
    search; backed by `<filesystem>` / `polycpp::fs`.
  - `path.basename`, `dirname`, `extname`, `resolve` — backed by
    `polycpp::path`.
  - timers are not used.
- crypto, compression, TLS, network, and HTTP APIs: not applicable — none
  appear upstream.
- unsupported Node-specific APIs and audit reason: `process.defaultApp`
  (Electron-specific, used only to skip `argv[0]` for Electron bundles) is
  detected at runtime when `polycpp::process::env("ELECTRON_RUN_AS_NODE")`
  or equivalent is observable; otherwise the Electron "from" mode is the
  caller's responsibility (`parse(argv, {.from = "electron"})`).

## External SDK and native driver strategy

- upstream services or protocols touched: not applicable — commander is a
  CLI argument parser. It does not talk to any remote system; it only
  reads `argv`/`env` and writes formatted help text to stdout/stderr.
- native SDKs/client libraries to use: not applicable.
- SDKs/protocols explicitly not reimplemented: not applicable.
- adapter/linking strategy: not applicable.
- test environment needs: none beyond a POSIX shell for the
  executable-subcommand tests.

## Compatibility foundation review

- downstream dependency role: `commander` is a leaf in the JS dependency
  graph for most CLI apps. It is itself a foundation for many CLIs, but not
  for other libraries.
- native substitution risk: low. There is no equivalent native SDK to
  replace; the package *is* the implementation.
- upstream implementation data to preserve: option-value source ordering
  (`default` < `config` < `env` < `cli` < `implied`), the camelCase
  conversion rules, the `--no-*` negation rules, the help layout column
  widths, and the suggestion edit-distance threshold (Damerau-Levenshtein
  with a 0.4 ratio cap). All preserved by direct port.
- generated or vendored data plan: not applicable — commander has no
  generated tables, codecs, or data files.
- compatibility fixture strategy: the upstream `tests/*.test.js` files are
  the fixtures. The C++ tests under `tests/test_*.cpp` mirror the upstream
  grouping (argument, option, command, help, suggest_similar, error,
  integration). Direct line-by-line port is preferred where the JS test
  exercises pure parsing or formatting.

## Security and fail-closed review

- security-sensitive behavior: low. commander does not handle credentials,
  tokens, crypto, or untrusted network input. It does spawn child processes
  for stand-alone executable subcommands; that path is the only attack
  surface.
- trust boundary: `argv` and `process.env` are user-controlled. The library
  must not interpret them as shell input.
- supported protocol or algorithm matrix: not applicable.
- unsupported behavior and fail-closed policy: bounded executable-subcommand search, no-shell argv composition, unknown-option/excess-argument/mandatory-missing all fail closed by default. Details:
  - executable subcommand search refuses to spawn anything outside the
    declared `executableDir` (or, by default, the directory of `argv[1]`).
  - the executable name is composed as `<programName>-<subcommandName>` and
    is passed to `child_process::spawn` without shell interpolation.
  - unknown options fail closed (error exit) unless `allowUnknownOption(true)`
    is explicitly set.
  - excess arguments fail closed unless `allowExcessArguments(true)` is set.
  - mandatory missing options fail closed.
- result-set/framing drain policy: not applicable.
- binary payload type-mapping policy: not applicable.
- key, secret, credential, or user-controlled input handling: option values
  read from `process.env` are stored as opaque `JsonValue` strings; the
  library does not log or print them unless the application asks
  (e.g., via `--verbose`). Help output never echoes env-resolved values.
- misuse cases that must be tested: suggestion output sanitization, bounded `executableDir` traversal, `exitOverride` throw-instead-of-exit. Details:
  - unknown option / unknown command suggestions do not echo arbitrary user
    input back without escaping (covered by `test_suggest_similar.cpp`).
  - `executableDir` traversal is bounded (covered by integration tests).
  - `exitOverride()` correctly throws `CommanderError` instead of calling
    `process::exit`, so a host application can catch parse failures.

## Core use cases

- declare a CLI with options, arguments, and subcommands using a fluent API.
- parse `argv`, dispatch to an action handler, get typed option values.
- print formatted help with `--help` automatically and on demand
  (`outputHelp`, `helpInformation`).
- get a "did you mean?" suggestion when the user types an unknown option or
  command.
- bind options to environment variables.
- run an async action and `await` it via `parseAsync()`.
- spawn a stand-alone executable subcommand (`<prog>-<sub>`).

## Key features to port first

All of the following are in scope for v0 and implemented:

- `Command`, `Option`, `Argument`, `Help`, `CommanderError`,
  `InvalidArgumentError`
- option flag parsing, including `<value>`, `[value]`, `<values...>`,
  `--no-*`, short-flag combination, and `--flag=value`
- argument parsing, including `<required>`, `[optional]`, `<variadic...>`,
  defaults, choices, custom parsers
- subcommands (inline action and stand-alone executable)
- env-var binding, default/preset values, choices, conflicts, implies
- mandatory and required options
- camelCase attribute names
- help formatting, sort options/subcommands, addHelpText, custom Help
  subclassing, ANSI color suppression
- "did you mean?" suggestions on unknown options/commands
- lifecycle hooks: `preSubcommand`, `preAction`, `postAction`
- `parseAsync()` plus `actionAsync()` and `hookAsync()` overloads
- `exitOverride()` for host-application embedding
- `allowUnknownOption`, `allowExcessArguments`,
  `enablePositionalOptions`, `passThroughOptions`,
  `combineFlagAndOptionalValue`

## Features to defer

- Windows-specific stand-alone executable lookup (`.cmd`, `.bat`, `node
  --inspect` argv rewriting). Recorded in `docs/divergences.md`.
- `program.on('command:*', ...)` JavaScript-only event aliases that have no
  C++ equivalent. Replaced by typed event names on the EventEmitter base.

## v0 scope

- port version: 0.1.0
- versioning note: port version is independent from upstream versioning
- supported APIs: see "Key features to port first" above
- unsupported APIs: see `docs/divergences.md` "Deferred Features" and
  "Unsupported Runtime-Specific Features"
- dependency plan: zero direct dependencies. Only base `polycpp` is linked
  (`target_link_libraries(polycpp_commander PUBLIC polycpp)`).
- polycpp modules to use: `process`, `events`, `core` (JsonValue, Promise,
  Error), `child_process`, `fs`, `path`
- missing polycpp primitives: none. Every required primitive is present in
  the snapshot recorded above.
