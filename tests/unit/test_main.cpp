#include "test_harness.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

struct TestOptions {
  bool listSuites = false;
  bool showHelp = false;
  std::vector<std::string> suites;
};

constexpr std::string_view SuitePrefix = "primemanifest.";

auto normalizeSuite(std::string_view suite) -> std::string {
  if (suite.rfind(SuitePrefix, 0) == 0) {
    suite.remove_prefix(SuitePrefix.size());
  }
  return std::string(suite);
}

void printUsage(const char* argv0) {
  std::cout << "Usage: " << argv0
            << " [--test-suite <suite>] [--test-suite=<suite>] [--list-suites]\n";
}

auto parseArgs(int argc, char** argv, TestOptions& options) -> bool {
  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);
    if (arg == "--help" || arg == "-h") {
      options.showHelp = true;
      continue;
    }
    if (arg == "--list-suites") {
      options.listSuites = true;
      continue;
    }
    if (arg == "--test-suite") {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for --test-suite\n";
        return false;
      }
      options.suites.emplace_back(argv[++i]);
      continue;
    }
    constexpr std::string_view kSuiteArg = "--test-suite=";
    if (arg.rfind(kSuiteArg, 0) == 0) {
      auto value = std::string(arg.substr(kSuiteArg.size()));
      if (value.empty()) {
        std::cerr << "Missing value for --test-suite\n";
        return false;
      }
      options.suites.push_back(std::move(value));
      continue;
    }
    std::cerr << "Unknown argument: " << arg << "\n";
    return false;
  }
  return true;
}

auto collectGroups(std::vector<PrimeManifestTest::TestCase> const& tests)
    -> std::unordered_map<std::string, int> {
  std::unordered_map<std::string, int> groupCounts;
  for (auto const& test : tests) {
    groupCounts[std::string(test.group)]++;
  }
  return groupCounts;
}

} // namespace

int main(int argc, char** argv) {
  TestOptions options;
  if (!parseArgs(argc, argv, options)) {
    printUsage(argv[0]);
    return 2;
  }
  if (options.showHelp) {
    printUsage(argv[0]);
    return 0;
  }

  auto tests = PrimeManifestTest::registry();
  std::sort(tests.begin(), tests.end(), [](auto const& a, auto const& b) {
    if (std::string_view(a.group) != std::string_view(b.group)) {
      return std::string_view(a.group) < std::string_view(b.group);
    }
    return std::string_view(a.name) < std::string_view(b.name);
  });

  auto groupCounts = collectGroups(tests);
  for (auto const& entry : groupCounts) {
    if (entry.second > 100) {
      PrimeManifestTest::fail(__FILE__, __LINE__, "test group exceeds 100 tests");
    }
  }

  if (options.listSuites) {
    std::vector<std::string> groups;
    groups.reserve(groupCounts.size());
    for (auto const& entry : groupCounts) {
      groups.push_back(entry.first);
    }
    std::sort(groups.begin(), groups.end());
    for (auto const& group : groups) {
      std::cout << SuitePrefix << group << "\n";
    }
    return 0;
  }

  std::unordered_set<std::string> allowedGroups;
  if (!options.suites.empty()) {
    allowedGroups.reserve(options.suites.size());
    for (auto const& suite : options.suites) {
      allowedGroups.insert(normalizeSuite(suite));
    }
    bool missingSuite = false;
    for (auto const& suite : options.suites) {
      auto normalized = normalizeSuite(suite);
      if (groupCounts.find(normalized) == groupCounts.end()) {
        std::cerr << "Unknown test suite: " << suite << "\n";
        missingSuite = true;
      }
    }
    if (missingSuite) {
      std::cerr << "Available suites:\n";
      std::vector<std::string> groups;
      groups.reserve(groupCounts.size());
      for (auto const& entry : groupCounts) {
        groups.push_back(entry.first);
      }
      std::sort(groups.begin(), groups.end());
      for (auto const& group : groups) {
        std::cerr << "  " << SuitePrefix << group << "\n";
      }
      return 1;
    }
  }

  for (auto const& test : tests) {
    if (!allowedGroups.empty() &&
        allowedGroups.find(std::string(test.group)) == allowedGroups.end()) {
      continue;
    }
    test.fn();
  }

  if (PrimeManifestTest::failure_count() == 0) {
    std::cout << "All tests passed.\n";
  }
  return PrimeManifestTest::failure_count() == 0 ? 0 : 1;
}
