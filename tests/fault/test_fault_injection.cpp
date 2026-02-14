#include "engine/engine.h"
#include "fault/fault_injector.h"
#include "rules/implausible_value_rule.h"
#include "rules/threshold_rule.h"

#include <gtest/gtest.h>

using namespace kestrel;

static SensorReading make_reading(const std::string& id, double value, bool valid = true) {
    SensorReading r;
    r.sensor_id = id;
    r.value = value;
    r.valid = valid;
    r.timestamp = std::chrono::steady_clock::now();
    return r;
}

TEST(FaultInjector, NoFaultPassesThrough) {
    FaultInjector injector;
    auto reading = make_reading("s", 0.5);
    auto result = injector.apply(reading);
    EXPECT_DOUBLE_EQ(result.value, 0.5);
    EXPECT_TRUE(result.valid);
}

TEST(FaultInjector, InvalidValueFault) {
    FaultInjector injector;
    injector.inject("s", FaultType::INVALID_VALUE, {-1.0, 0, 0});

    auto reading = make_reading("s", 0.5);
    auto result = injector.apply(reading);
    EXPECT_DOUBLE_EQ(result.value, -1.0);
}

TEST(FaultInjector, InterfaceFailureFault) {
    FaultInjector injector;
    injector.inject("s", FaultType::INTERFACE_FAILURE, {});

    auto reading = make_reading("s", 0.5);
    auto result = injector.apply(reading);
    EXPECT_FALSE(result.valid);
}

TEST(FaultInjector, SpikeIsOneShot) {
    FaultInjector injector;
    injector.inject("s", FaultType::SPIKE, {0.99, 0, 0});

    auto r1 = injector.apply(make_reading("s", 0.5));
    EXPECT_DOUBLE_EQ(r1.value, 0.99);

    // Second apply should pass through (spike cleared)
    auto r2 = injector.apply(make_reading("s", 0.5));
    EXPECT_DOUBLE_EQ(r2.value, 0.5);
}

TEST(FaultInjector, MissingUpdateSuppressesCycles) {
    FaultInjector injector;
    FaultParameters params;
    params.suppress_cycles = 2;
    injector.inject("s", FaultType::MISSING_UPDATE, params);

    auto r1 = injector.apply(make_reading("s", 0.5));
    EXPECT_FALSE(r1.valid);

    auto r2 = injector.apply(make_reading("s", 0.5));
    EXPECT_FALSE(r2.valid);

    // Third apply: cycles exhausted, fault cleared
    auto r3 = injector.apply(make_reading("s", 0.5));
    EXPECT_TRUE(r3.valid);
    EXPECT_DOUBLE_EQ(r3.value, 0.5);
}

TEST(FaultInjector, ClearRemovesFault) {
    FaultInjector injector;
    injector.inject("s", FaultType::INTERFACE_FAILURE, {});
    EXPECT_TRUE(injector.has_fault("s"));

    injector.clear("s");
    EXPECT_FALSE(injector.has_fault("s"));

    auto result = injector.apply(make_reading("s", 0.5));
    EXPECT_TRUE(result.valid);
}

// Integration: fault injection -> engine state transition
TEST(FaultIntegration, FaultCausesStateTransition) {
    Engine engine;
    engine.add_rule(std::make_unique<ImplausibleValueRule>(-1.0, 200.0));

    FaultInjector injector;

    // Normal reading -> OK
    auto r1 = make_reading("s", 0.5);
    engine.process({injector.apply(r1)});
    EXPECT_EQ(engine.sensor_state("s"), SystemState::OK);

    // Inject fault -> FAILED
    injector.inject("s", FaultType::INVALID_VALUE, {999.0, 0, 0});
    auto r2 = make_reading("s", 0.5);
    engine.process({injector.apply(r2)});
    EXPECT_EQ(engine.sensor_state("s"), SystemState::FAILED);

    // Clear fault -> recovery to OK
    injector.clear("s");
    auto r3 = make_reading("s", 0.5);
    engine.process({injector.apply(r3)});
    EXPECT_EQ(engine.sensor_state("s"), SystemState::OK);
}
