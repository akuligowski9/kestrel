#include "fault/fault_injector.h"

#include <thread>

namespace kestrel {

void FaultInjector::inject(const std::string& sensor_id, FaultType type,
                           FaultParameters params) {
    ActiveFault fault;
    fault.type = type;
    fault.params = params;
    fault.cycles_remaining = params.suppress_cycles;
    faults_[sensor_id] = fault;
}

void FaultInjector::clear(const std::string& sensor_id) {
    faults_.erase(sensor_id);
}

void FaultInjector::clear_all() {
    faults_.clear();
}

SensorReading FaultInjector::apply(const SensorReading& reading) {
    auto it = faults_.find(reading.sensor_id);
    if (it == faults_.end()) {
        return reading;
    }

    auto& fault = it->second;
    SensorReading modified = reading;

    switch (fault.type) {
        case FaultType::INVALID_VALUE:
            modified.value = fault.params.injected_value;
            break;

        case FaultType::DELAYED_READING:
            std::this_thread::sleep_for(
                std::chrono::milliseconds(fault.params.delay_ms));
            break;

        case FaultType::MISSING_UPDATE:
            if (fault.cycles_remaining > 0) {
                --fault.cycles_remaining;
                modified.valid = false;
            } else {
                faults_.erase(it);
            }
            break;

        case FaultType::SPIKE:
            modified.value = fault.params.injected_value;
            // Spike is one-shot, clear after applying
            faults_.erase(it);
            break;

        case FaultType::INTERFACE_FAILURE:
            modified.valid = false;
            break;
    }

    return modified;
}

bool FaultInjector::has_fault(const std::string& sensor_id) const {
    return faults_.find(sensor_id) != faults_.end();
}

} // namespace kestrel
