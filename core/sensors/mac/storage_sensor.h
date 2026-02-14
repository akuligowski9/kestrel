#pragma once

#include "sensors/sensor.h"

namespace kestrel {

// Reports root volume disk utilization as a ratio (0.0 = empty, 1.0 = full).
class StorageSensor : public ISensor {
public:
    SensorReading read() override;
    std::string id() const override { return "storage"; }
};

} // namespace kestrel
