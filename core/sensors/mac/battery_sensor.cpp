#include "sensors/mac/battery_sensor.h"

#include <array>
#include <cstdio>
#include <string>

namespace kestrel {

SensorReading BatterySensor::read() {
    SensorReading reading;
    reading.sensor_id = id();
    reading.timestamp = std::chrono::steady_clock::now();

    // Use pmset to get battery percentage
    std::array<char, 256> buffer{};
    FILE* pipe = popen("pmset -g batt 2>/dev/null", "r");
    if (!pipe) {
        reading.valid = false;
        return reading;
    }

    std::string output;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    pclose(pipe);

    // Parse percentage from output like "100%; charged"
    auto pct_pos = output.find('%');
    if (pct_pos == std::string::npos || pct_pos < 1) {
        reading.valid = false;
        return reading;
    }

    // Walk backward to find start of number
    auto num_start = pct_pos;
    while (num_start > 0 && (std::isdigit(output[num_start - 1]) || output[num_start - 1] == '.')) {
        --num_start;
    }

    try {
        // Normalize from 0-100 to 0.0-1.0 to match other sensor scales
        reading.value = std::stod(output.substr(num_start, pct_pos - num_start)) / 100.0;
        reading.valid = true;
    } catch (...) {
        reading.valid = false;
    }

    return reading;
}

} // namespace kestrel
