# Kestrel

**A C++ embedded-style system monitor that watches system behavior and signals when conditions deviate from expected bounds.**

Kestrel is a system health monitoring and verification platform that continuously observes hardware-exposed system metrics, detects degraded behavior, and validates recovery through fault injection using an embedded-style architecture.

## Why "Kestrel"

A kestrel is a small bird of prey known for hovering in place while continuously observing its surroundings, acting only when conditions change.

This project reflects the same philosophy:

- Continuous observation without interference
- Explicit signaling when abnormal conditions occur
- Stable and predictable behavior under changing conditions

Kestrel observes system state rather than controlling it, mirroring monitoring and verification components commonly found in embedded and safety-critical systems.

## Motivation

Embedded and medical systems operate under constraints different from general application software:

- Hardware inputs may be delayed, corrupted, or unavailable
- Systems must continue operating under degraded conditions
- Behavior must be explainable and verifiable
- Failures must be explicit and bounded

Modern operating systems expose hardware state through system interfaces. While access mechanisms differ between platforms, the underlying problems remain consistent: periodic sampling, imperfect inputs, timing constraints, and fault detection and recovery.

Kestrel explores these concerns using a laptop environment while maintaining architectural patterns common to embedded Linux systems.

## Architecture

```
[ OS Hardware Interfaces ]
      (macOS APIs / system tools)
              |
[ C++ Kestrel Core Engine ]
              |
[ Optional UI Layer (macOS Menu Bar) ]
```

The system is composed of three layers:

**Kestrel Core (C++)** -- All system logic: sensor polling, data normalization, rule evaluation, system state management, fault injection, and structured logging. Runs independently from any UI as a CLI application.

**OS Sensor Adapter Layer** -- Platform-specific hardware access isolated behind a provider interface, keeping system logic portable.

**Optional macOS Status Bar UI** -- A lightweight Swift-based menu bar app that reads output from the core and displays current health state. Contains no system logic.

## System States

Kestrel explicitly models system health using four states:

| State | Meaning |
|---|---|
| `OK` | Valid data within expected bounds |
| `DEGRADED` | Delayed, missing, or inconsistent input |
| `FAILED` | Sensor unusable or invalid |
| `UNKNOWN` | Insufficient data available |

## Core Components

| Component | Responsibility |
|---|---|
| **Sensor** | Read raw values, timestamp data, report success/failure |
| **SensorManager** | Schedule polling intervals, collect outputs, forward to engine |
| **Engine** | Maintain measurement window, evaluate rules, determine health state |
| **RuleEvaluator** | Threshold violations, missing data detection, rate-of-change checks |
| **Logger** | Structured logging (JSONL), state transitions, fault events |

## Fault Injection

Kestrel includes a fault injection system for controlled verification:

- Invalid sensor values
- Delayed readings
- Missing sensor updates
- Sudden value spikes
- Simulated interface failure

The system is expected to detect anomalies, transition to the appropriate degraded or failed state, log the event, and recover when conditions normalize.

## Building

Requirements: C++17 compiler (Clang), CMake 3.20+, Ninja

```bash
cmake -B build -G Ninja
cmake --build build
```

## Running

```bash
./build/kestrel          # run with default sensor configuration
./build/kestrel --fault   # run with fault injection enabled
```

## Testing

```bash
cd build && ctest
```

## Project Structure

```
kestrel/
  core/
    sensors/       # sensor implementations and adapter interface
    engine/        # central engine and state management
    rules/         # rule evaluation logic
    main.cpp       # entry point
  macos-app/       # optional Swift menu bar UI
  tests/           # unit and fault verification tests
  docs/            # architecture and experiment documentation
  configs/         # sensor and fault injection profiles
  CMakeLists.txt
```

## Design Principles

- Predictability over cleverness
- Explicit system states
- Bounded resource usage
- Clear separation of concerns
- Verification as a first-class feature

## License

MIT
