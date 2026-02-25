# Contributing to Kestrel

Thank you for your interest in contributing.

Kestrel is an embedded-style system monitor for macOS. Contributions should maintain the strict layer separation between OS interfaces, the C++ core engine, and the Swift menu bar UI.

---

## Code of Conduct

Be kind, respectful, and constructive. We're building something useful together — treat fellow contributors the way you'd want to be treated. Harassment, dismissive behavior, and unconstructive criticism have no place here.

---

## New to Contributing?

If this is your first open source contribution, welcome! Here's how to get started:

1. **Find an issue** — Look for issues labeled [`good first issue`](https://github.com/akuligowski9/kestrel/labels/good%20first%20issue) for beginner-friendly tasks.
2. **Fork the repo** — Click "Fork" on GitHub, then clone your fork locally.
3. **Create a branch** — See [Branch Naming](#branch-naming) below.
4. **Make your changes** — Follow the setup instructions and run tests before submitting.
5. **Open a PR** — Push your branch and open a pull request against `main`.

If you're new to Git and GitHub, [GitHub's guide](https://docs.github.com/en/get-started/quickstart/contributing-to-projects) is a great place to start.

---

## Issue Etiquette

- **Comment before you start** — If you'd like to work on an issue, leave a comment so others know it's being tackled. This avoids duplicate effort.
- **Ask questions in the issue thread** — If you're stuck or unsure about the approach, ask! We're happy to help.
- **Don't go silent** — If you claimed an issue but can't finish it, that's totally fine. Just leave a comment so someone else can pick it up.

---

## Development Setup

### Prerequisites

- macOS 13+
- Xcode (for Swift)
- CMake + Ninja (`brew install cmake ninja`)

### Quick Start

```bash
# Clone the repo
git clone https://github.com/akuligowski9/kestrel.git
cd kestrel

# Build the C++ core
cmake -B build -G Ninja && cmake --build build

# Run C++ tests
cd build && ctest --output-on-failure

# Run Swift tests
swift test --package-path macos-app
```

Run both test suites before submitting a PR.

---

## Architecture

Keep the layer separation intact:

```
[ OS Hardware Interfaces ] → [ C++ Core Engine ] → [ Swift Menu Bar UI ]
```

- Sensors read and report — they don't make decisions
- Rules are stateless and independently testable
- The engine is deterministic beyond its measurement window
- The UI reads JSONL from the core's stdout — it never drives behavior

---

## Code Standards

- **C++ core**: No external dependencies beyond GoogleTest and nlohmann/json (both fetched via CMake FetchContent)
- **Swift UI**: Menu bar app contains no system logic — all logic lives in the C++ core
- **Sensors**: All sensor values normalized to 0.0–1.0 before entering the engine
- **State transitions**: Must be logged with timestamp, previous state, new state, and reason
- **Tests**: New features and bug fixes should include tests

---

## Project Structure

- `core/` — C++ engine, sensors, rules, fault injection, logging
- `macos-app/` — Swift menu bar app (KestrelBar) and testable library (KestrelBarLib)
- `tests/` — C++ unit and fault injection tests
- `configs/` — Sensor and fault injection JSON profiles
- `docs/` — Technical specification and verification log

---

## Branch Naming

Use a descriptive branch name with a prefix:

- `feature/linux-sensors`
- `fix/battery-normalization`
- `docs/update-readme`

Keep it short, lowercase, and hyphen-separated.

---

## Commit Messages

- Use the imperative mood: "Add sensor" not "Added sensor"
- Keep the first line under 72 characters
- Add a blank line before any extended description

---

## Pull Request Guidelines

Please ensure:

- Both test suites pass (C++ and Swift)
- Code is readable without AI context
- Layer separation is maintained (sensors don't make decisions, UI doesn't drive behavior)
- New sensors normalize values to 0.0–1.0
- State transitions are logged
- Changes are documented if behavior changes

Small, focused PRs are preferred.

---

## Reporting Issues

Open an issue on GitHub with:

- What you expected to happen
- What actually happened
- Steps to reproduce
- macOS version and hardware (Apple Silicon / Intel)

---

## AI-Assisted Contributions

AI-assisted contributions are welcome.

Please review and understand generated code before submitting.
Maintainers may request clarification if behavior is unclear.
