#pragma once

#include <string>
#include <vector>

struct TestCase {
    std::string name;
    void (*func)();
};

struct AssertionFailure {
    std::string expr;
    const char* file;
    int line;
};

std::vector<TestCase>& test_registry();
bool register_test(const std::string& name, void (*func)());

#define TEST_CASE(name)                         \
    void name();                                \
    static bool name##_registered = register_test(#name, &name); \
    void name()

#define REQUIRE(expr)                                                    \
    do {                                                                 \
        if (!(expr)) {                                                   \
            throw AssertionFailure{#expr, __FILE__, __LINE__};           \
        }                                                                \
    } while (0)

