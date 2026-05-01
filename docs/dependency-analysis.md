# Dependency and JavaScript API Analysis

- package: commander
- package version: 14.0.3
- package root: `<repo path>/.tmp/upstream/commander.js`
- analyzer json: `.tmp/dependency-analysis.json`
- published npm artifact path: `<repo path>/.tmp/npm-package`
- published npm artifact analyzed: yes
- include dev dependencies: no
- dependency source install used: not required — commander has zero runtime dependencies, so `npm install --ignore-scripts` produced an empty `node_modules`
- companion root checked: `<companion libs root>/`

## Package entry metadata

- main: `./index.js`
- module: not declared (CJS package; ESM bridge via `exports['./esm.mjs']`)
- types: `typings/index.d.ts`
- exports: `.` (require → `./index.js`, types → `./typings/index.d.ts`),
  `./esm.mjs` (default → `./esm.mjs`, types → `./typings/esm.d.mts`)
- bin: none
- missing declared entries in repo clone: none — every declared entry exists
  in the source clone.
- TypeScript source files detected: 4 (`typings/index.d.ts`,
  `typings/esm.d.mts`, plus two `tests/` typing fixtures used only for the
  upstream `tsd` test harness).
- TypeScript declarations reviewed: yes. The TypeScript declarations were
  the cleanest reference for overload shapes; they show the public split
  between sync `action(fn)` and async `action(fn): Promise<...>` (commander
  collapses both into one in JS — the C++ port keeps them separate as
  `action()` and `actionAsync()`).
- declaration-source decision: `typings/index.d.ts` is the declaration source
  of truth for public API shape. The C++ port mirrors its overload set.
- source-vs-published artifact decision: source clone is the runtime source
  of truth (commander ships JS files unchanged — there is no transpile or
  bundle step). The published npm artifact and the source tree are
  byte-identical for `lib/*.js` and `index.js`.

## Direct dependencies

- none detected
- explanation: `package.json` declares no `dependencies`,
  `peerDependencies`, or `optionalDependencies`. commander is fully
  self-contained.

## Dependency ownership decisions

Not applicable — there are no runtime dependencies to own.

| Package | Kind | Requested | Installed | License | License evidence | License impact | License strategy | Affects repo license | Deps | Source files | Node API calls | JS API calls | Recommendation | Rationale |
|---|---|---|---|---|---|---|---|---|---:|---:|---:|---:|---|---|

(table intentionally has no body rows — see "Direct dependencies" above:
zero runtime dependencies, so there is nothing to classify.)

## License impact summary

- upstream package license: MIT (`package.json` `"license": "MIT"`, confirmed
  by upstream `LICENSE` file)
- repo license decision: MIT (this companion ships under MIT — see
  `LICENSE`)
- GPL/AGPL dependencies: none
- LGPL/MPL dependencies: none
- permissive dependencies requiring notices: none (zero deps)
- dev/test-only dependencies excluded from shipped artifacts: not applicable
  — only base `polycpp` and GoogleTest (test-only) are linked, both already
  permissively licensed.
- dependency license notices to add to `THIRD_PARTY_LICENSES.md`: only the
  upstream commander.js MIT notice (recorded in this repo's
  `THIRD_PARTY_LICENSES.md`).

## Transitive dependency summary

- transitive count: 0 — `npm install --ignore-scripts --omit=dev` against
  upstream produces an empty `node_modules`.

## Runtime API usage

### Target package

- entry points analyzed: `index.js` (and the `lib/*.js` files it re-exports)
- source files analyzed by analyzer: 7 (`index.js`, `lib/argument.js`,
  `lib/command.js`, `lib/error.js`, `lib/help.js`, `lib/option.js`,
  `lib/suggestSimilar.js`)
- source files manually inspected: same 7 plus `typings/index.d.ts`
- external imports seen from target: `node:child_process`, `node:events`,
  `node:fs`, `node:path`, `node:process` (all Node built-ins; no userland
  packages)

### Analyzer porting gates

- polycpp reuse hints consumed: yes — see "Polycpp ecosystem reuse analysis"
  in `docs/research.md`. Every hinted module (`process`, `events`, `fs`,
  `path`, `JSON`) maps to the correspondingly named polycpp module.
- Node parity hints consumed: yes — six surfaces flagged
  (callbacks, promises, event-emitter, buffer-binary, timers-process,
  filesystem-path). Each is resolved in `docs/research.md` "Node parity
  surface audit" and `docs/api-mapping.md` "Node parity surface review".
- security hints consumed: yes — analyzer reported
  `securitySensitive: false`, `cryptoApis: []`. Manually confirmed: no
  credential, token, crypto, or untrusted network input handling.
- security-sensitive package: no — but the spawn path (executable
  subcommands) is reviewed in `docs/research.md` "Security and fail-closed
  review" because it is the only externally observable side effect.
- polycpp capability snapshot consumed: yes — recorded in
  `docs/research.md` (HEAD `4ed62f96…`, checked 2026-05-01).
- transport/listener capability hints consumed: not applicable — commander
  has no socket/listener surface.

### Node.js API usage

The analyzer reported 37 distinct Node API call sites. Grouped by module:

- `process` (23 sites): `argv`, `argv[0]`, `defaultApp`, `env`,
  `env.CLICOLOR_FORCE`, `env.FORCE_COLOR`, `env.NO_COLOR`, `execArgv`,
  `execPath`, `exit`, `exitCode`, `on`, `platform`, `stderr.{columns,
  hasColors, isTTY, write}`, `stdout.{columns, hasColors, isTTY, write}`,
  `versions.electron`. → mapped to `polycpp::process::*` and
  `OutputConfiguration` callbacks.
- `child_process` (5 sites): `spawn`, `ChildProcess.kill`,
  `ChildProcess.on`. → mapped to `polycpp::child_process::spawn` and the
  signal-forwarding glue in `Command::executeSubCommand_`.
- `events` (1 site): `EventEmitter`. → `Command` extends
  `polycpp::events::EventEmitter`.
- `fs` (4 sites): `existsSync`, `realpathSync`. → mapped to `<filesystem>`
  / `polycpp::fs`.
- `path` (5 sites): `basename`, `dirname`, `extname`, `resolve`. → mapped to
  `polycpp::path`.

### Node parity surface usage

- callbacks: action handlers, hooks, output-configuration callbacks,
  exit-override callback. All preserved as `std::function` in C++.
- Promise APIs: `parseAsync()`, async actions and async hooks. Mapped to
  `polycpp::Promise<>`. Sync handlers are wrapped in
  immediately-resolving promises, mirroring upstream.
- EventEmitter APIs: `Command extends EventEmitter`. `option:<flag>`,
  `optionEnv:<flag>`, `command:*` events on the JS side; the C++ port emits
  the same event names so user listeners attached via `prog.on(...)` see
  them.
- server/listener APIs: not applicable.
- diagnostic/tracing APIs: not applicable.
- streams: not applicable. `process.stdout.write(buf)` is replaced by the
  `OutputConfiguration::writeOut(const std::string&)` callback (default:
  `std::cout`).
- Buffer and binary data: not applicable beyond a single defensive
  `Buffer.isBuffer` check upstream that has no observable C++ effect; not
  exposed in the C++ public API.
- URL/timer/process/filesystem APIs: `process` and `fs`/`path` covered
  above; no URL or timer use.
- crypto/compression/TLS/network/HTTP APIs: not applicable.

### JavaScript API usage

The analyzer reported 41 distinct standard-library JS API call sites
covering `Array.prototype.*`, `Map`/`Map.prototype.*`,
`Object.{assign, keys, entries}`, `String.prototype.*`, `Error`,
`JSON.stringify`, and `Promise.prototype.then`. All are standard-library
operations with direct C++ equivalents:

- `Array.prototype.{forEach, map, filter, find, includes, push, slice,
  sort, reverse, concat, join}` → `<algorithm>` and `std::vector` member
  functions.
- `Map`, `Map.prototype.{get, has, set, delete, forEach, keys, values}` →
  `std::unordered_map` / `std::map`.
- `Object.{assign, keys, entries}` not used on the public API surface
  beyond merging help configuration — handled directly in C++.
- `Error`, `Error.captureStackTrace` → C++ exceptions
  (`polycpp::Error`-derived types). `captureStackTrace` is a Node-only
  observability hook with no C++ equivalent; not preserved.
- `JSON.stringify` → users can call `polycpp::JSON::stringify(opts())`
  directly because `opts()` already returns a `JsonValue`.
- `Promise.prototype.then` → covered by `polycpp::Promise` chaining.

### Framework object boundary usage

- analyzer-reported target-package framework object accesses: 7 reads on a
  `context` object inside `Help`: `context.command`, `context.error`,
  `context.hasColors`, `context.helpWidth`, `context.write`. These are all
  reads of the `Help.PrepareContext`-style object that `Help` builds
  internally before formatting.
- analyzer-reported dependency framework object accesses: 0 (no dependencies).
- manual review decision: the `context` object becomes a typed C++ struct
  (`PrepareContextOptions`) plus references to the surrounding
  `OutputConfiguration` and `Command`. The C++ adapter boundary is the
  pair of overloads `Help::prepareContext(int helpWidth)` and
  `Help::prepareContext(const PrepareContextOptions&)`. There is no
  duck-typed object on the public surface.

## Porting decisions

- zero runtime dependencies → zero C++ porting cost on the dependency
  axis. Build only requires base `polycpp`.
- Node built-in usage maps cleanly to `polycpp::process`,
  `polycpp::events::EventEmitter`, `polycpp::child_process`, `polycpp::fs`,
  `polycpp::path`, `polycpp::JsonValue`, `polycpp::Promise`, and
  `polycpp::Error`. No new private helpers are needed for built-in parity.
- Local typed structs (`OutputConfiguration`, `ParseOptions`,
  `ParseOptionsResult`, `HelpContext`, `ErrorOptions`, `CommandOptions`,
  `PrepareContextOptions`, `SplitFlags`) exist only as plain structs whose
  fields directly mirror an upstream options-bag, so a `JsonObject` wrapper
  would be strictly worse.
- All porting decisions are consistent with the ecosystem reuse decisions
  recorded in `docs/research.md`.

## Analyzer warnings

- none emitted by analyzer (top-level `warnings: []`, target-package
  `warnings: []`).
