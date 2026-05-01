// A multi-command CLI: `todo add <title>`, `todo list`, `todo done <id>`.
// Tasks live in-memory for the life of the process — swap the in-process
// store for a real one when you adapt this to your app.
//
//   $ ./todo add "write docs"
//   added #1: write docs
//   $ ./todo add --priority 1 "ship release"
//   added #2: ship release (priority 1)
//   $ ./todo list
//   [ ] #1 write docs          (priority 2)
//   [ ] #2 ship release        (priority 1)
//   $ ./todo done 1
//   completed #1

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <polycpp/commander/commander.hpp>
#include <polycpp/process.hpp>

namespace {

struct Task {
    int id;
    std::string title;
    int priority;
    bool done;
};

std::vector<Task>& store() {
    static std::vector<Task> v;
    return v;
}

int nextId() {
    static int id = 0;
    return ++id;
}

}  // namespace

int main(int argc, char** argv) {
    polycpp::process::init(argc, argv);

    auto& prog = polycpp::commander::program();
    prog.name("todo")
        .description("a tiny task tracker")
        .version("0.1.0")
        .showSuggestionAfterError(true)
        .showHelpAfterError("(use --help for usage)");

    prog.option("-v, --verbose", "log every action");

    prog.command("add <title>")
        .description("add a new task")
        .option("-p, --priority <n>", "1 = high, 3 = low",
                [](const std::string& s, const polycpp::JsonValue&) {
                    return polycpp::JsonValue(std::stoi(s));
                },
                polycpp::JsonValue(2))
        .action([](const auto& args, const auto& opts, auto& cmd) {
            auto t = Task{nextId(), args[0].asString(),
                          opts["priority"].asInt(), false};
            store().push_back(t);
            const auto verbose = cmd.optsWithGlobals()["verbose"];
            if (verbose.isBool() && verbose.asBool())
                std::cerr << "[verbose] stored #" << t.id << '\n';
            std::cout << "added #" << t.id << ": " << t.title;
            if (t.priority != 2) std::cout << " (priority " << t.priority << ")";
            std::cout << '\n';
        });

    prog.command("list")
        .description("list all tasks")
        .action([](const auto&, const auto&, auto&) {
            for (const auto& t : store())
                std::cout << (t.done ? "[x] " : "[ ] ")
                          << '#' << t.id << ' '
                          << std::left << std::setw(18) << t.title
                          << " (priority " << t.priority << ")\n";
        });

    prog.command("done <id>")
        .description("mark a task done")
        .action([](const auto& args, const auto&, auto& cmd) {
            int id = std::stoi(args[0].asString());
            for (auto& t : store())
                if (t.id == id) {
                    t.done = true;
                    std::cout << "completed #" << id << '\n';
                    return;
                }
            cmd.error("no such task: " + std::to_string(id));
        });

    prog.parse();
    return 0;
}
