# Contributing to Kestrel

## Getting Started

1. Fork the repository and clone your fork
2. Install dependencies: `brew install cmake ninja`
3. Build: `cmake -B build -G Ninja && cmake --build build`
4. Run tests before making changes to confirm a clean baseline

## Running Tests

All tests must pass before submitting a PR.

```bash
# C++ tests (40)
cd build && ctest --output-on-failure

# Swift tests (36)
swift test --package-path macos-app
```

## Submitting Changes

1. Create a branch from `main`
2. Make your changes
3. Run both test suites and confirm they pass
4. Push your branch and open a pull request against `main`
5. PRs require at least 1 approving review before merge

## Code Standards

- **C++ core**: No external dependencies beyond GoogleTest and nlohmann/json (both fetched via CMake FetchContent)
- **Swift UI**: Menu bar app contains no system logic — all logic lives in the C++ core
- **Sensors**: All sensor values normalized to 0.0–1.0 before entering the engine
- **State transitions**: Must be logged with timestamp, previous state, new state, and reason
- **Tests**: New features and bug fixes should include tests

## Architecture

Keep the layer separation intact:

```
[ OS Hardware Interfaces ] → [ C++ Core Engine ] → [ Swift Menu Bar UI ]
```

- Sensors read and report — they don't make decisions
- Rules are stateless and independently testable
- The engine is deterministic beyond its measurement window
- The UI reads JSONL from the core's stdout — it never drives behavior

## Project Structure

- `core/` — C++ engine, sensors, rules, fault injection, logging
- `macos-app/` — Swift menu bar app (KestrelBar) and testable library (KestrelBarLib)
- `tests/` — C++ unit and fault injection tests
- `configs/` — Sensor and fault injection JSON profiles
- `docs/` — Technical specification and verification log

## Reporting Issues

Open an issue on GitHub with:

- What you expected to happen
- What actually happened
- Steps to reproduce
- macOS version and hardware (Apple Silicon / Intel)
