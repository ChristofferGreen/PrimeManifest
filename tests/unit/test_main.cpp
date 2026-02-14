#include "test_harness.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

int main() {
  auto tests = PrimeManifestTest::registry();
  std::sort(tests.begin(), tests.end(), [](auto const& a, auto const& b) {
    if (std::string_view(a.group) != std::string_view(b.group)) {
      return std::string_view(a.group) < std::string_view(b.group);
    }
    return std::string_view(a.name) < std::string_view(b.name);
  });

  std::unordered_map<std::string, int> groupCounts;
  for (auto const& test : tests) {
    groupCounts[std::string(test.group)]++;
  }
  for (auto const& entry : groupCounts) {
    if (entry.second > 100) {
      PrimeManifestTest::fail(__FILE__, __LINE__, "test group exceeds 100 tests");
    }
  }

  for (auto const& test : tests) {
    test.fn();
  }

  if (PrimeManifestTest::failure_count() == 0) {
    std::cout << "All tests passed.\n";
  }
  return PrimeManifestTest::failure_count() == 0 ? 0 : 1;
}
