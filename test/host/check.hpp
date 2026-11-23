#pragma once
// Minimal test harness shared by the host test executables: CHECK, a test registry and a runner.
#include <cstdio>
#include <functional>
#include <vector>

namespace check {

inline int& failed() {
    static int n = 0;
    return n;
}

struct Test {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Test>& registry() {
    static std::vector<Test> tests;
    return tests;
}

struct Register {
    Register(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

// Runs every registered test; prints failures and a summary. Returns the process exit code.
inline int run_all(const char* suite) {
    for (const auto& t : registry()) {
        int before = failed();
        t.fn();
        std::printf("%s %s\n", failed() == before ? "  ok  " : "  FAIL", t.name);
    }
    std::printf(failed() ? "%s: %d check(s) FAILED\n" : "%s: all checks passed\n", suite, failed());
    return failed() ? 1 : 0;
}

}  // namespace check

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++check::failed();                                           \
        }                                                                \
    } while (0)

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
// TEST(name) { ... } defines and registers a test.
#define TEST(name)                                                              \
    static void name();                                                         \
    static check::Register CHECK_CAT(reg_, name)(#name, name);                  \
    static void name()
