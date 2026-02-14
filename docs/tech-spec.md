# Kestrel Technical Specification

**Embedded-Style System Health Monitoring & Verification Platform**

Version: 1.0
Status: Draft

---

## 1. Overview

Kestrel is a system health monitoring and verification platform designed to simulate embedded-system behavior using hardware interfaces available on a standard laptop.

The system continuously collects measurements from operating system-exposed hardware interfaces (power, CPU load, memory, storage activity, network state), processes them in real time, and evaluates system behavior under both normal and degraded conditions.

The project emphasizes:

- Deterministic behavior
- Explicit failure handling
- Verification through fault injection
- Separation between hardware access, system logic, and user interface

Kestrel is not intended to be a production monitoring tool. Its purpose is to model embedded-style system behavior and verification practices using a portable architecture.

## 2. System Architecture

### 2.1 Layer Diagram

```
[ OS Hardware Interfaces ]
      (macOS APIs / system tools)
              |
              v
[ C++ Kestrel Core Engine ]
              |
              v
[ Optional UI Layer (macOS Menu Bar) ]
```

### 2.2 Kestrel Core (C++)

The core is the primary system component and contains all system logic.

Responsibilities:

- Sensor polling and scheduling
- Data normalization
- Rule evaluation
- System state management
- Fault injection
- Logging and verification output

The core runs independently from any UI and can operate as a CLI application. This mirrors embedded systems where core behavior exists independently from display or interface layers.

### 2.3 OS Sensor Adapter Layer

Sensor access is isolated behind an adapter interface:

```
ISensorProvider
    +-- MacSensorProvider
    +-- (Future) LinuxSensorProvider
```

This provides:

- Platform-specific hardware access behind a stable interface
- Portable system logic
- Clear mapping between OS interfaces

**macOS implementation targets:**

| Metric | Source |
|---|---|
| Battery / power status | `pmset`, IOKit |
| CPU / memory statistics | `sysctl` |
| Storage utilization | `statfs` |
| Network interface state | System Configuration framework |

Linux equivalents (`/proc`, `/sys`, `upower`) are documented for future portability but not implemented in the initial version.

### 2.4 Optional macOS Status Bar Interface

A lightweight Swift-based menu bar application that:

- Displays current system health state
- Provides quick metric visibility
- Triggers system notifications on state changes

Constraints:

- Contains no system logic
- Reads output from the Kestrel Core
- Acts only as a visualization and notification mechanism

This mirrors embedded devices where UI layers are separate from system behavior.

## 3. System States

Kestrel explicitly models system health using an enumerated state model:

```
enum class SystemState {
    OK,
    DEGRADED,
    FAILED,
    UNKNOWN
};
```

| State | Definition |
|---|---|
| `OK` | Valid data within expected bounds |
| `DEGRADED` | Delayed, missing, or inconsistent input |
| `FAILED` | Sensor unusable or invalid |
| `UNKNOWN` | Insufficient data available |

Explicit states prevent undefined behavior and simplify verification. State transitions are logged and testable.

### 3.1 State Transition Rules

- `UNKNOWN -> OK`: First valid reading received within bounds
- `UNKNOWN -> DEGRADED`: First reading received but outside expected range or timing
- `OK -> DEGRADED`: Reading delayed, missing, or inconsistent
- `OK -> FAILED`: Sensor reports error or value is clearly invalid
- `DEGRADED -> OK`: Consistent valid readings resume
- `DEGRADED -> FAILED`: Condition worsens beyond degraded threshold
- `FAILED -> DEGRADED`: Sensor begins returning data again but not yet stable
- `FAILED -> OK`: Sensor returns stable valid readings (may require multiple consecutive valid samples)

All state transitions are logged with timestamp, previous state, new state, and trigger reason.

## 4. Core Component Specification

The core system limits complexity to five primary components.

### 4.1 Sensor

Responsible only for reading raw values.

```cpp
struct SensorReading {
    double value;
    std::chrono::steady_clock::time_point timestamp;
    bool valid;
    std::string sensor_id;
};

class ISensor {
public:
    virtual ~ISensor() = default;
    virtual SensorReading read() = 0;
    virtual std::string id() const = 0;
};
```

Implementations:

- `BatterySensor` -- charge level, charging state, discharge rate
- `CpuLoadSensor` -- system and user CPU utilization
- `MemorySensor` -- physical memory usage, swap pressure
- `StorageSensor` -- disk utilization, available space

Sensors do not make decisions. They read, timestamp, and report.

### 4.2 SensorManager

Coordinates sensor polling.

```cpp
class SensorManager {
public:
    void register_sensor(std::unique_ptr<ISensor> sensor,
                         std::chrono::milliseconds interval);
    std::vector<SensorReading> poll();
};
```

Responsibilities:

- Schedule polling intervals per sensor
- Collect sensor outputs
- Forward samples to engine
- Enforce bounded polling frequency (no sensor polled faster than configured minimum)

### 4.3 Engine

Central system logic.

```cpp
class Engine {
public:
    void process(const std::vector<SensorReading>& readings);
    SystemState current_state() const;
    std::vector<StateTransition> recent_transitions() const;
};
```

Responsibilities:

- Maintain recent measurement window (bounded circular buffer)
- Evaluate rules against current window
- Determine per-sensor and aggregate system health state
- Handle state transitions
- Emit events for logging

The Engine is deterministic and stateless beyond its measurement window.

### 4.4 RuleEvaluator

Encapsulates anomaly detection logic.

```cpp
class IRule {
public:
    virtual ~IRule() = default;
    virtual RuleResult evaluate(const MeasurementWindow& window) = 0;
    virtual std::string name() const = 0;
};
```

Rule types:

| Rule | Description |
|---|---|
| `ThresholdRule` | Value exceeds configured upper/lower bound |
| `MissingDataRule` | No reading received within expected interval |
| `ImplausibleValueRule` | Value outside physically possible range |
| `RateOfChangeRule` | Value changes faster than expected rate |

Rules are explicit, stateless, and independently testable.

### 4.5 Logger

Records system behavior in structured format.

```cpp
class Logger {
public:
    void log_reading(const SensorReading& reading);
    void log_transition(const StateTransition& transition);
    void log_fault(const FaultEvent& event);
    void log_rule_violation(const RuleResult& result);
};
```

Output format: JSONL (one JSON object per line)

```json
{"ts":"2026-02-13T12:00:00Z","type":"reading","sensor":"cpu_load","value":0.72,"valid":true}
{"ts":"2026-02-13T12:00:01Z","type":"transition","sensor":"battery","from":"OK","to":"DEGRADED","reason":"reading_delayed"}
{"ts":"2026-02-13T12:00:02Z","type":"fault","fault_type":"spike","sensor":"cpu_load","injected_value":99.9}
```

Logs support verification, reproducibility, and post-run analysis.

## 5. Fault Injection System

Fault injection allows controlled verification of system behavior under adverse conditions.

### 5.1 Supported Fault Types

| Fault | Description |
|---|---|
| `InvalidValue` | Inject a value outside valid range (e.g., -1.0 for battery) |
| `DelayedReading` | Delay sensor response beyond expected interval |
| `MissingUpdate` | Suppress sensor output for N cycles |
| `Spike` | Inject sudden value jump (e.g., CPU 0.1 -> 0.99) |
| `InterfaceFailure` | Simulate complete sensor unavailability |

### 5.2 Fault Injection Interface

```cpp
class FaultInjector {
public:
    void inject(const std::string& sensor_id, FaultType type,
                FaultParameters params);
    void clear(const std::string& sensor_id);
    void clear_all();
};
```

Faults are injected at the sensor adapter layer, between the real sensor and the engine, preserving the integrity of the core logic path.

### 5.3 Expected Behavior Under Fault

For each injected fault, the system must:

1. Detect the anomaly through rule evaluation
2. Transition to the appropriate state (`DEGRADED` or `FAILED`)
3. Log the event with fault context
4. Recover to `OK` when the fault is cleared and valid readings resume

## 6. Experiments

Kestrel supports repeatable experiments to observe system behavior under controlled conditions.

### 6.1 Experiment A -- Sustained Compute Load

Run compute-intensive workloads while measuring:

- CPU load profile
- Power consumption behavior
- Thermal response (if accessible via IOKit)

Goal: Observe system stability and anomaly detection under sustained load.

### 6.2 Experiment B -- Battery Discharge Behavior

Monitor battery under controlled workload:

- Discharge rate over time
- Power state transitions (AC -> battery, low battery thresholds)

Goal: Validate sensor accuracy and state transition behavior during real hardware changes.

### 6.3 Experiment C -- Storage Activity

Controlled I/O workload:

- Disk utilization patterns under sequential and random write
- Latency changes under load

Goal: Observe storage sensor behavior and rule evaluation under I/O pressure.

### 6.4 Experiment Structure

Each experiment produces:

- A JSONL log of all readings, transitions, and events
- A summary of state transitions observed
- Pass/fail criteria based on expected behavior

## 7. Verification Strategy

Verification is a core design goal, not an afterthought.

### 7.1 Unit Verification

- Sensor reading parsing and normalization
- Rule evaluation correctness (each rule tested with known inputs)
- State transition logic (all valid transitions verified, invalid transitions rejected)
- Measurement window bounds (buffer does not grow unbounded)

### 7.2 Fault Verification

- Each fault type triggers the expected state transition
- System recovers correctly after fault is cleared
- No crashes, deadlocks, or undefined states during fault injection
- Multiple simultaneous faults are handled correctly

### 7.3 Stability Verification

- Long-duration runs (30+ minutes) without memory growth
- CPU overhead remains bounded and predictable
- No file descriptor leaks
- Log output remains well-formed over extended runs

### 7.4 Tooling

| Tool | Purpose |
|---|---|
| CMake + Ninja | Build system |
| clang-tidy | Static analysis |
| AddressSanitizer | Memory error detection during development |
| ThreadSanitizer | Data race detection (if multithreaded) |
| ctest | Test runner |

## 8. Configuration

Sensor and fault profiles are defined in JSON configuration files.

### 8.1 Sensor Configuration

```json
{
  "sensors": [
    {
      "id": "cpu_load",
      "type": "CpuLoadSensor",
      "poll_interval_ms": 1000,
      "bounds": { "min": 0.0, "max": 1.0 }
    },
    {
      "id": "battery",
      "type": "BatterySensor",
      "poll_interval_ms": 5000,
      "bounds": { "min": 0.0, "max": 100.0 }
    }
  ]
}
```

### 8.2 Fault Profile

```json
{
  "faults": [
    {
      "sensor_id": "cpu_load",
      "type": "Spike",
      "trigger_after_s": 10,
      "duration_s": 5,
      "value": 0.99
    }
  ]
}
```

## 9. Assumptions

- Sensors update at least once per configured interval under normal conditions
- Invalid values are detectable via configured bounds
- Missing updates within the expected interval indicate degraded state
- System runs in a standard macOS user-space environment
- No root or kernel-level access is required

## 10. Project Structure

```
kestrel/
  core/
    sensors/          # ISensor interface and platform implementations
    engine/           # Engine, state management, measurement window
    rules/            # IRule interface and rule implementations
    fault/            # FaultInjector and fault type definitions
    logging/          # Logger and JSONL output
    main.cpp          # CLI entry point
  macos-app/          # Optional Swift menu bar UI
  tests/
    unit/             # Unit tests for each component
    fault/            # Fault injection verification tests
    stability/        # Long-running stability tests
  docs/
    tech-spec.md      # This document
    architecture.md   # Diagrams and design notes
    experiments.md    # Experiment procedures and results
  configs/
    default.json      # Default sensor configuration
    fault-basic.json  # Basic fault injection profile
  CMakeLists.txt
```

## 11. Non-Goals

- Production monitoring or alerting
- Cross-platform support in initial version (macOS only)
- Real-time guarantees (best-effort timing in user-space)
- Network-based monitoring or remote telemetry
- GUI beyond the optional menu bar status display

## 12. Future Considerations

These are documented for context but explicitly out of scope for the initial build:

- Linux sensor provider implementation
- Remote log streaming
- Configuration hot-reload
- Additional sensor types (GPU, Bluetooth, external USB devices)
- Experiment automation and comparison tooling
