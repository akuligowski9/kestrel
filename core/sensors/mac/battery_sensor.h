#pragma once

#include "sensors/sensor.h"

namespace kestrel {

class BatterySensor : public ISensor {
public:
    SensorReading read() override;
    std::string id() const override { return "battery"; }
};

} // namespace kestrel
