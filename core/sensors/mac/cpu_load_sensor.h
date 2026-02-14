#pragma once

#include "sensors/sensor.h"

namespace kestrel {

class CpuLoadSensor : public ISensor {
public:
    SensorReading read() override;
    std::string id() const override { return "cpu_load"; }
};

} // namespace kestrel
