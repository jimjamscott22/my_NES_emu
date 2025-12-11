#include "test_framework.h"

#include <iostream>

std::vector<TestCase>& test_registry() {
    static std::vector<TestCase> tests;
    return tests;
}

bool register_test(const std::string& name, void (*func)()) {
    test_registry().push_back({name, func});
    return true;
}

int main() {
    int failures = 0;
    auto& tests = test_registry();

    for (const auto& test : tests) {
        try {
            test.func();
            std::cout << "[PASS] " << test.name << "\n";
        } catch (const AssertionFailure& a) {
            ++failures;
            std::cout << "[FAIL] " << test.name << " (" << a.file << ":"
                      << a.line << ") " << a.expr << "\n";
        } catch (...) {
            ++failures;
            std::cout << "[FAIL] " << test.name
                      << " (unexpected exception)\n";
        }
    }

    std::cout << "Ran " << tests.size() << " test(s). Failures: " << failures
              << "\n";
    return failures == 0 ? 0 : 1;
}

