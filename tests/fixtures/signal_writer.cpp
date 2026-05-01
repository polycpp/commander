/**
 * @file signal_writer.cpp
 * @brief Test fixture: wait for a forwarded signal, write its name to a file,
 *        then exit.
 *
 * This fixture is spawned by the polycpp_commander_test_executable suite to
 * observe how `polycpp::commander::Command::executableCommand(...)` forwards
 * parent-side signals to its child process.
 *
 * Behaviour:
 *  - Installs `sigaction` handlers for SIGTERM, SIGINT, SIGHUP, SIGUSR1, and
 *    SIGUSR2. Each handler:
 *      1. async-safe-writes the signal name (e.g. "SIGTERM\n") to the path
 *         pointed to by `$SIGNAL_WRITER_OUTPUT` (open-write-close, with
 *         `fsync` so the parent can read it after this process exits).
 *      2. Exits with code `128 + signum` (POSIX convention for signal exits).
 *  - Before entering the wait loop, writes "READY\n" to the path pointed to
 *    by `$SIGNAL_WRITER_READY` so the test thread knows when it's safe to
 *    deliver the signal.
 *  - Wait loop is `pause()` — blocks until any signal arrives.
 *
 * The fixture deliberately does NOT depend on polycpp_commander or polycpp;
 * it is a stand-alone executable that must be discoverable on disk by the
 * commander dispatch path under test.
 *
 * Async-safety: we do all signal-handler work via raw POSIX I/O
 * (`open`/`write`/`fsync`/`close`/`_exit`) and avoid `printf`, `std::ofstream`,
 * `std::string`, and any allocator that is not signal-safe.
 */

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace {

/// @brief Async-safe strlen.
size_t signalSafeStrlen(const char* s) {
    size_t n = 0;
    while (s && s[n] != '\0') ++n;
    return n;
}

/// @brief Async-safe write of `text` to the file at `$SIGNAL_WRITER_OUTPUT`.
/// Open-write-fsync-close so the parent observes the byte even when this
/// process is killed shortly after this call.
void writeOutput(const char* text) {
    const char* path = std::getenv("SIGNAL_WRITER_OUTPUT");
    if (!path || !*path) return;
    int fd = ::open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) return;
    size_t len = signalSafeStrlen(text);
    ssize_t total = 0;
    while (static_cast<size_t>(total) < len) {
        ssize_t n = ::write(fd, text + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        total += n;
    }
    ::fsync(fd);
    ::close(fd);
}

/// @brief Map a known signal number to its name; returns nullptr otherwise.
const char* nameOf(int signum) {
    switch (signum) {
        case SIGTERM: return "SIGTERM\n";
        case SIGINT:  return "SIGINT\n";
        case SIGHUP:  return "SIGHUP\n";
        case SIGUSR1: return "SIGUSR1\n";
        case SIGUSR2: return "SIGUSR2\n";
        default:      return nullptr;
    }
}

extern "C" void onSignal(int signum) {
    const char* name = nameOf(signum);
    if (name) writeOutput(name);
    // 128 + signum is the POSIX convention for "killed by signal N".
    ::_exit(128 + signum);
}

void installHandler(int signum) {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &onSignal;
    sa.sa_flags = 0; // no SA_RESTART so pause() returns
    ::sigemptyset(&sa.sa_mask);
    ::sigaction(signum, &sa, nullptr);
}

void writeReady() {
    const char* path = std::getenv("SIGNAL_WRITER_READY");
    if (!path || !*path) return;
    int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return;
    const char* msg = "READY\n";
    size_t len = signalSafeStrlen(msg);
    ssize_t total = 0;
    while (static_cast<size_t>(total) < len) {
        ssize_t n = ::write(fd, msg + total, len - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        total += n;
    }
    ::fsync(fd);
    ::close(fd);
}

} // namespace

int main(int /*argc*/, char** /*argv*/) {
    installHandler(SIGTERM);
    installHandler(SIGINT);
    installHandler(SIGHUP);
    installHandler(SIGUSR1);
    installHandler(SIGUSR2);

    writeReady();

    // Block until any signal arrives. pause() returns -1 with errno=EINTR
    // when interrupted by an unmasked signal (which is what we want).
    // The signal handler calls _exit() so this loop should never iterate
    // more than once in practice.
    while (true) {
        ::pause();
    }
    return 0;
}
