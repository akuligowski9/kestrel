#pragma once

#include <chrono>
#include <memory>
#include <string>

namespace kestrel {

struct SensorReading {
    double value = 0.0;
    std::chrono::steady_clock::time_point timestamp;
    bool valid = false;
    std::string sensor_id;
};

class ISensor {
public:
    virtual ~ISensor() = default;
    virtual SensorReading read() = 0;
    virtual std::string id() const = 0;
};

} // namespace kestrel
