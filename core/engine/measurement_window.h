#pragma once

#include "sensors/sensor.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace kestrel {

// Bounded circular buffer of recent readings per sensor.
class MeasurementWindow {
public:
    explicit MeasurementWindow(std::size_t capacity = 64);

    void push(const SensorReading& reading);

    // Returns readings for a given sensor, oldest first.
    std::vector<SensorReading> readings_for(const std::string& sensor_id) const;

    // Returns the most recent reading for a sensor, or nullopt-equivalent invalid reading.
    SensorReading latest(const std::string& sensor_id) const;

    std::size_t capacity() const { return capacity_; }

private:
    struct RingBuffer {
        std::vector<SensorReading> buffer;
        std::size_t head = 0;
        std::size_t count = 0;
    };

    std::size_t capacity_;
    std::unordered_map<std::string, RingBuffer> buffers_;
};

} // namespace kestrel
