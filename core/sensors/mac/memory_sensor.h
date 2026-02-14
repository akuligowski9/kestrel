#pragma once

#include "sensors/sensor.h"

namespace kestrel {

// Reports memory pressure as a ratio (0.0 = no pressure, 1.0 = fully utilized).
class MemorySensor : public ISensor {
public:
    SensorReading read() override;
    std::string id() const override { return "memory"; }
};

} // namespace kestrel
