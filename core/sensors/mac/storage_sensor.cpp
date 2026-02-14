#include "sensors/mac/storage_sensor.h"

#include <sys/mount.h>

namespace kestrel {

SensorReading StorageSensor::read() {
    SensorReading reading;
    reading.sensor_id = id();
    reading.timestamp = std::chrono::steady_clock::now();

    struct statfs stat;
    if (statfs("/", &stat) != 0) {
        reading.valid = false;
        return reading;
    }

    uint64_t total = static_cast<uint64_t>(stat.f_blocks) * stat.f_bsize;
    uint64_t available = static_cast<uint64_t>(stat.f_bavail) * stat.f_bsize;

    if (total == 0) {
        reading.valid = false;
        return reading;
    }

    reading.value = 1.0 - (static_cast<double>(available) / static_cast<double>(total));
    reading.valid = true;
    return reading;
}

} // namespace kestrel
