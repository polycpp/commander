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
