#pragma once

#include "sensor.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace kestrel {

class SensorManager {
public:
    void register_sensor(std::unique_ptr<ISensor> sensor,
                         std::chrono::milliseconds interval);

    // Poll all sensors whose interval has elapsed. Returns new readings.
    std::vector<SensorReading> poll();

private:
    struct SensorEntry {
        std::unique_ptr<ISensor> sensor;
        std::chrono::milliseconds interval;
        std::chrono::steady_clock::time_point last_poll;
    };

    std::vector<SensorEntry> entries_;
};

} // namespace kestrel
