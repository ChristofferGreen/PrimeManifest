#pragma once

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PrimeManifestTest {

struct TestCase {
  const char* group = "";
  const char* name = "";
  void (*fn)() = nullptr;
};

inline std::vector<TestCase>& registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline int& failure_count() {
  static int failures = 0;
  return failures;
}

inline void fail(const char* file, int line, const char* msg) {
  std::cerr << "FAIL: " << msg << " (" << file << ":" << line << ")\n";
  ++failure_count();
}

struct Registrar {
  Registrar(const char* group, const char* name, void (*fn)()) {
    registry().push_back(TestCase{group, name, fn});
  }
};

} // namespace PrimeManifestTest

#define PM_CONCAT_INNER(a, b) a##b
#define PM_CONCAT(a, b) PM_CONCAT_INNER(a, b)

#define PM_CHECK(cond, msg) do { \
  if (!(cond)) { \
    PrimeManifestTest::fail(__FILE__, __LINE__, msg); \
  } \
} while (0)

#define PM_TEST(group, name) \
  static void PM_CONCAT(pm_test_fn_, PM_CONCAT(group, PM_CONCAT(_, name)))(); \
  static PrimeManifestTest::Registrar \
      PM_CONCAT(pm_test_reg_, PM_CONCAT(group, PM_CONCAT(_, name)))(#group, #name, \
      &PM_CONCAT(pm_test_fn_, PM_CONCAT(group, PM_CONCAT(_, name)))); \
  static void PM_CONCAT(pm_test_fn_, PM_CONCAT(group, PM_CONCAT(_, name)))()
