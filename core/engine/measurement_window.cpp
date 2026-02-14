#include "engine/measurement_window.h"

#include <algorithm>

namespace kestrel {

MeasurementWindow::MeasurementWindow(std::size_t capacity)
    : capacity_(capacity) {}

void MeasurementWindow::push(const SensorReading& reading) {
    auto& rb = buffers_[reading.sensor_id];

    if (rb.buffer.size() < capacity_) {
        rb.buffer.push_back(reading);
        rb.head = rb.buffer.size() - 1;
        rb.count = rb.buffer.size();
    } else {
        rb.head = (rb.head + 1) % capacity_;
        rb.buffer[rb.head] = reading;
        rb.count = capacity_;
    }
}

std::vector<SensorReading> MeasurementWindow::readings_for(
    const std::string& sensor_id) const {

    auto it = buffers_.find(sensor_id);
    if (it == buffers_.end()) {
        return {};
    }

    const auto& rb = it->second;
    std::vector<SensorReading> result;
    result.reserve(rb.count);

    // Walk from oldest to newest
    std::size_t start = (rb.count < capacity_)
        ? 0
        : (rb.head + 1) % capacity_;

    for (std::size_t i = 0; i < rb.count; ++i) {
        result.push_back(rb.buffer[(start + i) % rb.buffer.size()]);
    }

    return result;
}

SensorReading MeasurementWindow::latest(const std::string& sensor_id) const {
    auto it = buffers_.find(sensor_id);
    if (it == buffers_.end() || it->second.count == 0) {
        SensorReading empty;
        empty.sensor_id = sensor_id;
        empty.valid = false;
        return empty;
    }
    return it->second.buffer[it->second.head];
}

} // namespace kestrel
