#include "sensors/mac/memory_sensor.h"

#include <mach/mach.h>
#include <sys/sysctl.h>

namespace kestrel {

SensorReading MemorySensor::read() {
    SensorReading reading;
    reading.sensor_id = id();
    reading.timestamp = std::chrono::steady_clock::now();

    // Get total physical memory
    int64_t total_mem = 0;
    size_t size = sizeof(total_mem);
    if (sysctlbyname("hw.memsize", &total_mem, &size, nullptr, 0) != 0) {
        reading.valid = false;
        return reading;
    }

    // Get VM statistics
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    kern_return_t kr = host_statistics64(
        mach_host_self(), HOST_VM_INFO64,
        reinterpret_cast<host_info64_t>(&vm_stat), &count);

    if (kr != KERN_SUCCESS) {
        reading.valid = false;
        return reading;
    }

    vm_size_t page_size;
    host_page_size(mach_host_self(), &page_size);

    // macOS aggressively caches into "inactive" and compressor pages.
    // Only active + wired + compressor-occupied pages represent real pressure.
    // Free, inactive, and speculative pages are all reclaimable.
    int64_t used_pages = static_cast<int64_t>(vm_stat.active_count)
                       + static_cast<int64_t>(vm_stat.wire_count)
                       + static_cast<int64_t>(vm_stat.compressor_page_count);
    int64_t used_mem = used_pages * static_cast<int64_t>(page_size);

    reading.value = static_cast<double>(used_mem) / static_cast<double>(total_mem);
    reading.valid = true;
    return reading;
}

} // namespace kestrel
