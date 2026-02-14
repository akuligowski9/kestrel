#pragma once

#include "sensors/sensor.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace kestrel {

enum class FaultType {
    INVALID_VALUE,
    DELAYED_READING,
    MISSING_UPDATE,
    SPIKE,
    INTERFACE_FAILURE
};

struct FaultParameters {
    double injected_value = 0.0;
    int suppress_cycles = 0;    // for MISSING_UPDATE
    int delay_ms = 0;           // for DELAYED_READING
};

// Wraps a real sensor and optionally injects faults into its readings.
class FaultInjector {
public:
    void inject(const std::string& sensor_id, FaultType type,
                FaultParameters params);
    void clear(const std::string& sensor_id);
    void clear_all();

    // Apply fault (if any) to a reading. Returns modified reading.
    SensorReading apply(const SensorReading& reading);

    bool has_fault(const std::string& sensor_id) const;

private:
    struct ActiveFault {
        FaultType type;
        FaultParameters params;
        int cycles_remaining = 0;
    };

    std::unordered_map<std::string, ActiveFault> faults_;
};

} // namespace kestrel
