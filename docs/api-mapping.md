# API Mapping

| Upstream symbol | C++ symbol | Status | Notes |
|---|---|---|---|
| `commander.program` | `polycpp::commander::program()` | direct | Returns reference to a process-lifetime singleton. |
| `commander.Command` | `polycpp::commander::Command` | direct | Extends `polycpp::events::EventEmitter`; non-copyable. |
| `commander.Option` | `polycpp::commander::Option` | direct | |
| `commander.Argument` | `polycpp::commander::Argument` | direct | |
| `commander.Help` | `polycpp::commander::Help` | direct | All methods virtual for subclassing. |
| `commander.CommanderError` | `polycpp::commander::CommanderError` | direct | Extends `polycpp::Error`; carries `exitCode` and `code`. |
| `commander.InvalidArgumentError` | `polycpp::commander::InvalidArgumentError` | direct | Subclass of `CommanderError`; throw from custom parsers. |
| `commander.createCommand(name)` | `polycpp::commander::createCommand(name)` | adapted | Returns `std::unique_ptr<Command>` (see Divergences) because `Command` is non-copyable. |
| `commander.createOption(flags, desc)` | `polycpp::commander::createOption(flags, desc)` | direct | Returns by value. |
| `commander.createArgument(name, desc)` | `polycpp::commander::createArgument(name, desc)` | direct | Returns by value. |
| `Command#name(str)` / `name()` | `Command::name(str)` / `Command::name()` | direct | Dual getter/setter. |
| `Command#description(str)` / `description()` | `Command::description(str)` / `Command::description()` | direct | Dual getter/setter. |
| `Command#summary(str)` / `summary()` | `Command::summary(str)` / `Command::summary()` | direct | Dual getter/setter. |
| `Command#usage(str)` / `usage()` | `Command::usage(str)` / `Command::usage()` | direct | Dual getter/setter. |
| `Command#version(str, flags?, desc?)` | `Command::version(str, flags?, desc?)` | direct | Registers `-V, --version` option. |
| `Command#alias(name)` / `aliases(arr)` | `Command::alias(name)` / `Command::aliases(vec)` | direct | |
| `Command#nameFromFilename(file)` | `Command::nameFromFilename(file)` | direct | Strips path/extension via `polycpp::path`. |
| `Command#option(flags, desc, default?)` | `Command::option(flags, desc, default?)` overloads | direct | Three overloads: bool, default, parser+default. |
| `Command#requiredOption(flags, desc, default?)` | `Command::requiredOption(...)` overloads | direct | Same shape as `option()`. |
| `Command#addOption(option)` | `Command::addOption(option)` | direct | Core method that other option setters delegate to. |
| `Command#createOption(flags, desc)` | `virtual Command::createOption(...) const` | direct | Virtual factory for subclassing. |
| `Command#argument(name, desc)` | `Command::argument(name, desc)` | direct | |
| `Command#arguments(spec)` | `Command::arguments(spec)` | direct | Space-separated multi-arg helper. |
| `Command#addArgument(arg)` | `Command::addArgument(arg)` | direct | |
| `Command#createArgument(name, desc)` | `virtual Command::createArgument(...) const` | direct | Virtual factory. |
| `Command#command(spec, opts?)` | `Command::command(spec, opts?)` | direct | Returns reference to the new subcommand. |
| `Command#command(spec, desc, opts?)` (executable form) | `Command::executableCommand(spec, desc, executableFile?)` | adapted | JS overloads on the same name; C++ splits into a separate explicit method to keep types unambiguous. |
| `Command#executableDir(path)` / `executableDir()` | `Command::executableDir(path)` / `Command::executableDir()` | direct | |
| `Command#addCommand(cmd, opts?)` | `Command::addCommand(unique_ptr<Command>, opts?)` | adapted | Takes ownership via `unique_ptr` because `Command` is non-copyable. |
| `Command#createCommand(name)` | `virtual Command::createCommand(name) const` | direct | Virtual factory. |
| `Command#action(fn)` | `Command::action(ActionFn)` | direct | Sync action handler. |
| `Command#action(asyncFn)` | `Command::actionAsync(AsyncActionFn)` | adapted | Split from sync overload for type clarity (see Divergences). |
| `Command#parse(argv?, opts?)` | `Command::parse(argv?, opts?)` overloads | direct | No-arg overload reads `polycpp::process::argv()`. |
| `Command#parseAsync(argv?, opts?)` | `Command::parseAsync(argv?, opts?)` | direct | Returns `polycpp::Promise<std::reference_wrapper<Command>>`. |
| `Command#parseOptions(args)` | `Command::parseOptions(args)` | direct | Returns `ParseOptionsResult { operands, unknown }`. |
| `Command#opts()` | `Command::opts()` | direct | Returns `JsonValue` (object-shaped). |
| `Command#optsWithGlobals()` | `Command::optsWithGlobals()` | direct | |
| `Command#getOptionValue(key)` | `Command::getOptionValue(key)` | direct | |
| `Command#setOptionValue(key, value)` | `Command::setOptionValue(key, value)` | direct | |
| `Command#setOptionValueWithSource(key, value, source)` | `Command::setOptionValueWithSource(key, value, source)` | direct | source ∈ {"default","config","env","cli","implied"}. |
| `Command#getOptionValueSource(key)` | `Command::getOptionValueSource(key)` | direct | |
| `Command#getOptionValueSourceWithGlobals(key)` | `Command::getOptionValueSourceWithGlobals(key)` | direct | |
| `Command#helpOption(flags, desc)` / `(false)` | `Command::helpOption(flags, desc)` / `helpOption(bool)` | direct | Two overloads. |
| `Command#helpCommand(spec, desc)` / `(bool)` | `Command::helpCommand(spec, desc)` / `helpCommand(bool)` | direct | Two overloads. |
| `Command#addHelpCommand(cmd)` | `Command::addHelpCommand(unique_ptr<Command>)` | adapted | Ownership via `unique_ptr`. |
| `Command#outputHelp(ctx?)` | `Command::outputHelp(ctx?)` | direct | |
| `Command#helpInformation(ctx?)` | `Command::helpInformation(ctx?)` | direct | |
| `Command#help(ctx?)` | `Command::help(ctx?)` | direct | Calls `outputHelp` and exits. |
| `Command#addHelpText(position, text)` | `Command::addHelpText(position, text)` | direct | position ∈ {"before","after","beforeAll","afterAll"}. |
| `Command#configureHelp(map)` / `configureHelp()` | `Command::configureHelp(map)` / `configureHelp()` | direct | |
| `Command#configureOutput(cfg)` / `configureOutput()` | `Command::configureOutput(OutputConfiguration)` / `configureOutput()` | direct | |
| `Command#showHelpAfterError(true or msg)` | `Command::showHelpAfterError(bool)` / `showHelpAfterError(string)` | direct | Two overloads. |
| `Command#showSuggestionAfterError(bool)` | `Command::showSuggestionAfterError(bool)` | direct | |
| `Command#error(message, opts?)` | `Command::error(message, opts?)` | direct | Throws or exits per `exitOverride()`. |
| `Command#exitOverride(fn?)` | `Command::exitOverride(fn?)` | direct | |
| `Command#hook(event, listener)` | `Command::hook(event, HookFn)` | direct | event ∈ {"preSubcommand","preAction","postAction"}. |
| `Command#hook(event, asyncListener)` | `Command::hookAsync(event, AsyncHookFn)` | adapted | Split from sync overload. |
| `Command#copyInheritedSettings(src)` | `Command::copyInheritedSettings(src)` | direct | |
| `Command#allowUnknownOption(bool)` | `Command::allowUnknownOption(bool)` | direct | |
| `Command#allowExcessArguments(bool)` | `Command::allowExcessArguments(bool)` | direct | |
| `Command#enablePositionalOptions(bool)` | `Command::enablePositionalOptions(bool)` | direct | |
| `Command#passThroughOptions(bool)` | `Command::passThroughOptions(bool)` | direct | |
| `Command#combineFlagAndOptionalValue(bool)` | `Command::combineFlagAndOptionalValue(bool)` | direct | |
| `Command#createHelp()` | `virtual Command::createHelp() const` | direct | Virtual factory. |
| `Option(flags, desc)` | `Option(flags, desc)` | direct | |
| `Option#name()`, `attributeName()` | `Option::name()`, `attributeName()` | direct | |
| `Option#isBoolean()`, `is(arg)` | `Option::isBoolean()`, `is(arg)` | direct | |
| `Option#default(value, desc?)` | `Option::defaultValue(value, desc?)` | adapted | Renamed to avoid keyword clash with C++ `default`. |
| `Option#preset(arg)` | `Option::preset(arg)` | direct | |
| `Option#conflicts(name or arr)` | `Option::conflicts(name)` / `conflicts(vec)` | direct | Two overloads. |
| `Option#implies(map)` | `Option::implies(map)` | direct | |
| `Option#env(name)` | `Option::env(name)` | direct | |
| `Option#argParser(fn)` | `Option::argParser(ParseFn)` | direct | |
| `Option#makeOptionMandatory(bool?)` | `Option::makeOptionMandatory(bool)` | direct | |
| `Option#hideHelp(bool?)` | `Option::hideHelp(bool)` | direct | |
| `Option#choices(arr)` | `Option::choices(vec)` | direct | |
| `Argument(name, desc)` | `Argument(name, desc)` | direct | |
| `Argument#name()` | `Argument::name()` | direct | |
| `Argument#default(value, desc?)` | `Argument::defaultValue(value, desc?)` | adapted | Same `default` keyword reason. |
| `Argument#argParser(fn)` | `Argument::argParser(ParseFn)` | direct | |
| `Argument#choices(arr)` | `Argument::choices(vec)` | direct | |
| `Argument#argRequired()` / `argOptional()` | `Argument::argRequired()` / `argOptional()` | direct | |
| `Help#helpWidth`, `sortOptions`, ... (read) | `Help::helpWidth()`, `Help::sortOptions()`, `Help::sortSubcommands()`, `Help::showGlobalOptions()`, `Help::outputHasColors()`, `Help::minWidthToWrap()` | direct | Const getters; dual getter/setter style matching `Command::name()`. The underlying fields are private. |
| `Help#helpWidth = N`, `sortOptions = b`, ... (write) | `Help::helpWidth(int)`, `Help::sortOptions(bool)`, `Help::sortSubcommands(bool)`, `Help::showGlobalOptions(bool)`, `Help::outputHasColors(bool)`, `Help::minWidthToWrap(int)` | direct | Fluent setters returning `Help&` for chaining. Replaces upstream's direct field assignment. |
| `Help#configureHelp({...})` (object form, applied through `Command#configureHelp`) | `Help::HelpConfiguration` + `Help::configure(HelpConfiguration)` | adapted | Typed struct with `std::optional` fields stands in for the untyped JS object; only engaged fields override. |
| `Help#prepareContext(width)` | `Help::prepareContext(width)` / `prepareContext(PrepareContextOptions)` | direct | Two overloads. |
| `Help#formatHelp(cmd, helper)` | `Help::formatHelp(cmd, helper)` | direct | |
| `Help#wrap(...)`, `padWidth(...)`, ... | identical names on `Help::*` | direct | |
| `suggestSimilar(word, candidates)` | `polycpp::commander::suggestSimilar(word, candidates)` | direct | Damerau-Levenshtein, ratio cap 0.4. |
| `humanReadableArgName(arg)` | `polycpp::commander::humanReadableArgName(arg)` | direct | |
| `splitOptionFlags(flags)` | `polycpp::commander::splitOptionFlags(flags)` | direct | Returns typed `SplitFlags`. |
| `camelcase(str)` | `polycpp::commander::camelcase(str)` | direct | |
| Windows `.cmd` / `.bat` lookup, `node --inspect` argv rewriting | (none) | deferred | Out of scope for v0; recorded in Divergences. |
| `Error.captureStackTrace` | (none) | omitted | Node-specific stack-capture hook, not meaningful in C++. |
| `process.defaultApp` (Electron) | `parse(argv, {.from = "electron"})` opt-in | adapted | Caller selects "electron" mode explicitly; runtime detection of Electron is out of scope. |

## TypeScript Declaration Review

- Declaration source used: `typings/index.d.ts` from upstream commander 14.0.3.
- Public APIs, overloads, options, callbacks, streams, or literal unions found
  only or most clearly in declarations:
  - the split between sync `action(cb)` and async `action(cb)` (typed as
    `(...): Promise<...>`); the C++ port mirrors this with separate
    `action()` and `actionAsync()` methods plus `hook()`/`hookAsync()`.
  - the literal-union `position: "before" | "after" | "beforeAll" | "afterAll"`
    for `addHelpText`.
  - the literal-union `from: "node" | "user" | "electron"` for
    `parse({from})` — modeled as `ParseOptions::from` validated at runtime.
  - the source-string union for `getOptionValueSource`:
    `"default" | "config" | "env" | "cli" | "implied" | undefined` —
    modeled as plain `std::string` with documented allowed values.
- Declaration-only globals, caches, deprecated fields, or runtime-specific
  surfaces mapped as unsupported/not-applicable: none — `typings/index.d.ts`
  is purely a public-API mirror.

## Framework object boundary review

- Upstream reads or mutates framework/request/response/context objects: yes,
  internal only — `Help.prepareContext()` returns a context object that is
  consumed by `Help.formatHelp()` and the `addHelpText` callback signature.
  No external `req`/`res`/`ctx` boundary.
- Upstream fields or methods read: `context.command`, `context.error`,
  `context.hasColors`, `context.helpWidth`, `context.write`.
- Upstream fields or methods written: none — the context is read-only after
  `prepareContext` builds it.
- C++ adapter boundary: typed `PrepareContextOptions` struct plus a
  reference to `OutputConfiguration` and `Command`. There is no duck-typed
  object on the public surface; users either subclass `Help` and override
  virtual methods, or pass a `PrepareContextOptions`.
- Partial mutation risk on validation failure: none — option values are
  validated by `Option::choices` / `Argument::choices` before
  `setOptionValueWithSource` is called, so a `choices` failure throws
  `InvalidArgumentError` before the value is stored.

## Node parity surface review

- Callback APIs: `ActionFn`, `HookFn`, `OutputConfiguration::write{Out,Err}`,
  `OutputConfiguration::outputError`, `OutputConfiguration::get{Out,Err}HelpWidth`,
  `OutputConfiguration::get{Out,Err}HasColors`, `Option::argParser` (ParseFn),
  `Argument::argParser` (ParseFn), `Command::exitOverride(fn)`. All
  preserved as `std::function`.
- Promise APIs: `Command::parseAsync` returns
  `polycpp::Promise<std::reference_wrapper<Command>>`; `actionAsync` and
  `hookAsync` accept `AsyncActionFn` / `AsyncHookFn` returning
  `polycpp::Promise<void>`. Sync handlers wrap into immediately-resolving
  promises so a single `parseAsync()` call can drive a mix.
- EventEmitter APIs: `Command extends polycpp::events::EventEmitter`. User
  listeners attached with `prog.on("option:verbose", cb)` see the same
  events upstream emits: `option:<flag>`, `optionEnv:<flag>`, plus the hook
  lifecycle events `preSubcommand` / `preAction` / `postAction` (these are
  also invocable through `Command::hook(event, fn)`).
- Server/listener APIs: not applicable.
- Diagnostic/tracing APIs: not applicable.
- Stream APIs: not applicable. Output is line-oriented through
  `OutputConfiguration::writeOut(const std::string&)` and
  `writeErr(const std::string&)`. Default routes to `std::cout`/`std::cerr`.
- Buffer and binary APIs: not applicable.
- URL, timer, process, and filesystem APIs: `polycpp::process` for
  `argv`/`exit`/`env`; `polycpp::path` for `basename`/`dirname`/`extname`/`resolve`;
  `polycpp::fs` (or `<filesystem>`) for `existsSync`/`realpathSync` during
  executable-subcommand search. No timer use.
- Crypto, compression, TLS, network, and HTTP APIs: not applicable.
- Unsupported or non-meaningful Node-specific APIs and audit reason: `Error.captureStackTrace`, automatic Electron detection via `process.defaultApp`, and Windows `.cmd`/`.bat` shim lookup with `node --inspect` argv splitting (the last is deferred, the first two are intentionally not preserved). Details:
  - `Error.captureStackTrace` — Node-only stack hook, no C++ equivalent.
  - `process.defaultApp` automatic Electron detection — replaced by
    explicit `parse(argv, {.from = "electron"})`.
  - Windows `.cmd` / `.bat` shim lookup and `node --inspect` argv splitting
    in stand-alone executable subcommand resolution — deferred.
