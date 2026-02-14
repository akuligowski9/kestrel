#include "sensors/mac/cpu_load_sensor.h"

#include <mach/mach.h>

namespace kestrel {

SensorReading CpuLoadSensor::read() {
    SensorReading reading;
    reading.sensor_id = id();
    reading.timestamp = std::chrono::steady_clock::now();

    host_cpu_load_info_data_t cpu_info;
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;

    kern_return_t kr = host_statistics(
        mach_host_self(), HOST_CPU_LOAD_INFO,
        reinterpret_cast<host_info_t>(&cpu_info), &count);

    if (kr != KERN_SUCCESS) {
        reading.valid = false;
        return reading;
    }

    // Calculate total and idle ticks
    unsigned long long total = 0;
    for (int i = 0; i < CPU_STATE_MAX; ++i) {
        total += cpu_info.cpu_ticks[i];
    }

    unsigned long long idle = cpu_info.cpu_ticks[CPU_STATE_IDLE];

    if (total == 0) {
        reading.valid = false;
        return reading;
    }

    reading.value = 1.0 - (static_cast<double>(idle) / static_cast<double>(total));
    reading.valid = true;
    return reading;
}

} // namespace kestrel
