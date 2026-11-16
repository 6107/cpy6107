#pragma once

// Minimal dependency-free test harness: TEST(name) registers a case, CHECK(expr) records a
// failure and continues, and RunAllTests() returns the number of failed tests.

#include <cstdio>
#include <vector>

namespace cpy6107::testing {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline int& FailureCount() {
    static int failures = 0;
    return failures;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

inline void ReportFailure(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "  CHECK FAILED %s:%d: %s\n", file, line, expr);
    ++FailureCount();
}

inline int RunAllTests() {
    int failed_tests = 0;
    for (const auto& test : Registry()) {
        const int before = FailureCount();
        std::printf("[ RUN  ] %s\n", test.name);
        std::fflush(stdout);
        test.fn();
        const bool ok = FailureCount() == before;
        std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", test.name);
        failed_tests += ok ? 0 : 1;
    }
    std::printf("%zu tests, %d failed\n", Registry().size(), failed_tests);
    return failed_tests;
}

} // namespace cpy6107::testing

#define TEST(name)                                                                                 \
    static void name();                                                                            \
    static const cpy6107::testing::Registrar name##_registrar{#name, &name};                       \
    static void name()

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            cpy6107::testing::ReportFailure(#expr, __FILE__, __LINE__);                            \
        }                                                                                          \
    } while (false)
