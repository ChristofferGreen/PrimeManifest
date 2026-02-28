# AGENTS.md for PrimeManifest

## Scope
Defines naming and coding rules plus build/test entrypoints for contributors working in this repo.

## Naming rules
- **Namespaces:** PascalCase for public namespaces (e.g., `PrimeManifest`).
- **Types/structs/classes/enums/aliases:** PascalCase.
- **Functions (free/member/static):** lowerCamelCase.
- **Struct/class fields:** lowerCamelCase.
- **Local/file-only helpers:** lower_snake_case is acceptable when marked `static` in a `.cpp`.
- **Constants (`constexpr`/`static`):** PascalCase; avoid `k`-prefixes.
- **Macros:** avoid new ones; if unavoidable, use `PM_` prefix and ALL_CAPS.

## Build/test workflow
- Debug: `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build-debug`.
- Release: `cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release` then `cmake --build build-release`.
- Options: `-DPRIMEMANIFEST_BUILD_TESTS=ON/OFF`, `-DPRIMEMANIFEST_BUILD_EXAMPLES=ON/OFF`.
- Tests: from a build dir, run `ctest --output-on-failure` or execute `./PrimeManifest_tests` directly.

## Tests
- Unit tests live in `tests/unit` and use doctest (`TEST_CASE`, `CHECK`, `TEST_SUITE_BEGIN`).
- Keep test suites under 100 cases.

## Code guidelines
- Target C++23; prefer value semantics, RAII, `std::span`, and `std::optional`/`std::expected`.
- Use `std::chrono` types for durations/timeouts.
- Keep renderer hot paths allocation-free; reuse buffers and caches where possible.
- Avoid raw `new`; use `std::unique_ptr` or `std::shared_ptr` with clear ownership intent.
- Keep source and test files around 700 lines or less when practical; split large files into focused units.

## Examples/benchmarks
- `renderer_bench` is the performance harness; `text_render_demo` is the rendering showcase.
- When recording benchmark imagery, use `--dump <file.ppm>` and keep `.ppm` alongside derived `.png` files.
