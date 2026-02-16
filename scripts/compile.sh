#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-debug"
BUILD_TYPE="Debug"
RUN_TESTS=1
CONFIGURE_ONLY=0
COVERAGE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --release)
      BUILD_DIR="$ROOT_DIR/build-release"
      BUILD_TYPE="Release"
      shift
      ;;
    --coverage)
      COVERAGE=1
      shift
      ;;
    --configure)
      CONFIGURE_ONLY=1
      shift
      ;;
    --skip-tests)
      RUN_TESTS=0
      shift
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 2
      ;;
  esac
 done

COVERAGE_CMAKE_ARGS=()

if command -v nproc >/dev/null 2>&1; then
  BUILD_JOBS="$(nproc)"
elif command -v sysctl >/dev/null 2>&1; then
  BUILD_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
else
  BUILD_JOBS=4
fi
if [[ $COVERAGE -eq 1 ]]; then
  if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "[compile.sh] Coverage: removing existing cache to ensure instrumentation." >&2
    rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles"
  fi
  if [[ $RUN_TESTS -eq 0 ]]; then
    echo "[compile.sh] ERROR: --coverage requires tests; remove --skip-tests." >&2
    exit 2
  fi
  if ! command -v clang++ >/dev/null 2>&1; then
    echo "[compile.sh] ERROR: --coverage requires clang++ in PATH." >&2
    exit 2
  fi
  COVERAGE_CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER=clang++")
  if command -v clang >/dev/null 2>&1; then
    COVERAGE_CMAKE_ARGS+=("-DCMAKE_C_COMPILER=clang")
  fi
  COVERAGE_CXX_FLAGS="${CXXFLAGS:-} -fprofile-instr-generate -fcoverage-mapping"
  COVERAGE_LINK_FLAGS="${LDFLAGS:-} -fprofile-instr-generate"
  COVERAGE_CMAKE_ARGS+=("-DCMAKE_CXX_FLAGS=${COVERAGE_CXX_FLAGS}")
  COVERAGE_CMAKE_ARGS+=("-DCMAKE_EXE_LINKER_FLAGS=${COVERAGE_LINK_FLAGS}")
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" "${COVERAGE_CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel "$BUILD_JOBS"

if [[ $CONFIGURE_ONLY -eq 1 ]]; then
  exit 0
fi

if [[ $RUN_TESTS -eq 1 ]]; then
  if [[ $COVERAGE -eq 1 ]]; then
    LLVM_PROFDATA="${LLVM_PROFDATA:-llvm-profdata}"
    LLVM_COV="${LLVM_COV:-llvm-cov}"
    if ! command -v "$LLVM_PROFDATA" >/dev/null 2>&1; then
      if command -v xcrun >/dev/null 2>&1; then
        LLVM_PROFDATA="$(xcrun --find llvm-profdata 2>/dev/null || true)"
      fi
    fi
    if ! command -v "$LLVM_COV" >/dev/null 2>&1; then
      if command -v xcrun >/dev/null 2>&1; then
        LLVM_COV="$(xcrun --find llvm-cov 2>/dev/null || true)"
      fi
    fi
    if [[ -z "$LLVM_PROFDATA" || ! -x "$LLVM_PROFDATA" ]]; then
      echo "[compile.sh] ERROR: llvm-profdata not found; install LLVM tools or set LLVM_PROFDATA." >&2
      exit 2
    fi
    if [[ -z "$LLVM_COV" || ! -x "$LLVM_COV" ]]; then
      echo "[compile.sh] ERROR: llvm-cov not found; install LLVM tools or set LLVM_COV." >&2
      exit 2
    fi

    report_dir="$BUILD_DIR/coverage"
    profile_dir="$report_dir/profraw"
    rm -rf "$report_dir"
    mkdir -p "$profile_dir"

    (cd "$BUILD_DIR" && LLVM_PROFILE_FILE="$profile_dir/%p-%m.profraw" ctest --output-on-failure)

    if ! compgen -G "$profile_dir/*.profraw" >/dev/null; then
      echo "[compile.sh] WARN: no .profraw files produced; coverage report skipped." >&2
      exit 0
    fi

    profdata="$report_dir/PrimeManifest.profdata"
    "$LLVM_PROFDATA" merge -sparse "$profile_dir"/*.profraw -o "$profdata"

    bins=("$BUILD_DIR/PrimeManifest_tests")
    for bin in "${bins[@]}"; do
      if [[ ! -x "$bin" ]]; then
        echo "[compile.sh] ERROR: coverage binary not found: $bin" >&2
        exit 2
      fi
    done

    coverage_ignore_regex="${PRIMEMANIFEST_COVERAGE_IGNORE_REGEX:-.*/(third_party|tests|examples|workspaces|out|tools)/?}"
    if [[ -n "$coverage_ignore_regex" ]]; then
      echo "[compile.sh] Coverage: ignoring files matching regex: $coverage_ignore_regex"
      coverage_args=(--ignore-filename-regex "$coverage_ignore_regex")
    else
      coverage_args=()
    fi

    text_report="$report_dir/coverage.txt"
    echo "[compile.sh] Generating coverage report -> $text_report"
    "$LLVM_COV" report "${coverage_args[@]}" "${bins[@]}" -instr-profile "$profdata" > "$text_report"

    echo "[compile.sh] Generating HTML coverage -> $report_dir/html"
    "$LLVM_COV" show "${coverage_args[@]}" "${bins[@]}" -instr-profile "$profdata" \
      -format=html -output-dir "$report_dir/html" -show-instantiations -show-line-counts-or-regions
  else
    (cd "$BUILD_DIR" && ctest --output-on-failure --progress)
  fi
fi
