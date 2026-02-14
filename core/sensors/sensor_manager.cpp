#include "sensor_manager.h"

namespace kestrel {

void SensorManager::register_sensor(std::unique_ptr<ISensor> sensor,
                                    std::chrono::milliseconds interval) {
    entries_.push_back({
        std::move(sensor),
        interval,
        std::chrono::steady_clock::time_point{} // force immediate first poll
    });
}

std::vector<SensorReading> SensorManager::poll() {
    std::vector<SensorReading> readings;
    auto now = std::chrono::steady_clock::now();

    for (auto& entry : entries_) {
        auto elapsed = now - entry.last_poll;
        if (elapsed >= entry.interval) {
            readings.push_back(entry.sensor->read());
            entry.last_poll = now;
        }
    }

    return readings;
}

} // namespace kestrel
