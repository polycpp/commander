Run async action handlers
=========================

**You'll build:** a ``fetch`` subcommand that performs HTTP calls inside
a ``polycpp::Promise<void>`` action handler, without blocking the
parser.

**You'll use:**
:cpp:func:`polycpp::commander::Command::actionAsync`,
:cpp:func:`polycpp::commander::Command::parseAsync`, and lifecycle
hooks via :cpp:func:`hookAsync()
<polycpp::commander::Command::hookAsync>`.

**Prerequisites:** familiarity with
:doc:`../tutorials/git-style-cli`, and polycpp's event loop / Promise
type.

Step 1 — pick the sync vs async boundary
----------------------------------------

Commander keeps sync and async APIs side by side. A lambda that just
pokes in-memory state should stay sync with :cpp:func:`action()
<polycpp::commander::Command::action>`; one that waits on I/O should
use :cpp:func:`actionAsync()
<polycpp::commander::Command::actionAsync>`:

.. code-block:: cpp

   sub.actionAsync([](auto, auto, auto&) -> polycpp::Promise<void> {
       co_await polycpp::http::request("https://example.com/api").body();
       co_return;
   });

:cpp:type:`AsyncActionFn <polycpp::commander::AsyncActionFn>` has the
same three-parameter shape as :cpp:type:`ActionFn
<polycpp::commander::ActionFn>`; only the return type changes.

Step 2 — drive the parser with ``parseAsync``
---------------------------------------------

:cpp:func:`parseAsync()
<polycpp::commander::Command::parseAsync>` returns a
``polycpp::Promise<std::reference_wrapper<Command>>`` that settles
*after* every async action handler and hook has resolved. Pair it with
the event loop:

.. code-block:: cpp

   int main(int argc, char** argv) {
       polycpp::process::init(argc, argv);
       auto& prog = polycpp::commander::program();

       prog.command("fetch <url>")
           .description("fetch a URL and print the body")
           .actionAsync([](const auto& args, const auto&, auto&)
                            -> polycpp::Promise<void> {
               auto res = co_await polycpp::http::get(args[0].asString());
               std::cout << co_await res.text();
           });

       auto fut = prog.parseAsync(polycpp::process::argv(), {.from = "native"});
       polycpp::EventLoop::instance().run();
       return 0;
   }

Without ``run()`` the event loop never wakes up and the promise stalls.

Step 3 — pre- and post-action hooks
-----------------------------------

Hooks fire around the action handler, so they're the right place for
timing, tracing, or resource cleanup:

.. code-block:: cpp

   auto start = std::make_shared<std::chrono::steady_clock::time_point>();

   prog.hookAsync("preAction",
       [start](auto&, auto&) -> polycpp::Promise<void> {
           *start = std::chrono::steady_clock::now();
           co_return;
       });

   prog.hookAsync("postAction",
       [start](auto&, auto& action) -> polycpp::Promise<void> {
           using ms = std::chrono::milliseconds;
           auto dt = std::chrono::duration_cast<ms>(
                         std::chrono::steady_clock::now() - *start);
           std::cerr << action.name() << " took " << dt.count() << " ms\n";
           co_return;
       });

The hook receives ``(thisCommand, actionCommand)``: ``thisCommand`` is
the one the hook was registered on, ``actionCommand`` is the (possibly
deeper) subcommand whose action is about to run.

Step 4 — mixing sync and async
------------------------------

You don't need to convert everything. A sync ``.action()`` gets wrapped
in an already-resolved promise by ``parseAsync``. This lets you add
async subcommands incrementally, leaving the rest of the CLI alone:

.. code-block:: cpp

   prog.command("version")
       .action([](auto, auto, auto&) {            // sync is fine
           std::cout << "todo 0.1.0\n";
       });

   prog.command("sync")
       .actionAsync([](auto, auto, auto&)         // async needs it
                         -> polycpp::Promise<void> {
           co_await sync::pushUpstream();
       });

What you learned
----------------

- ``actionAsync`` is the only change needed to opt into Promise-returning
  handlers.
- ``parseAsync`` must be paired with ``EventLoop::run()`` so pending
  async work actually executes.
- Sync and async actions coexist inside the same program — the parser
  wraps sync ones transparently.

Next: :doc:`custom-help` walks through tweaking the help renderer —
both by configuration and by subclassing :cpp:class:`Help
<polycpp::commander::Help>`.
