#include "engine/measurement_window.h"
#include "rules/implausible_value_rule.h"
#include "rules/missing_data_rule.h"
#include "rules/rate_of_change_rule.h"
#include "rules/threshold_rule.h"

#include <gtest/gtest.h>
#include <thread>

using namespace kestrel;

static SensorReading make_reading(const std::string& id, double value, bool valid = true) {
    SensorReading r;
    r.sensor_id = id;
    r.value = value;
    r.valid = valid;
    r.timestamp = std::chrono::steady_clock::now();
    return r;
}

// --- ThresholdRule ---

TEST(ThresholdRule, ValueWithinBoundsIsOK) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 0.5));

    ThresholdRule rule(0.0, 1.0);
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::OK);
}

TEST(ThresholdRule, ValueAboveUpperBound) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 1.5));

    ThresholdRule rule(0.0, 1.0);
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::DEGRADED);
}

TEST(ThresholdRule, ValueBelowLowerBound) {
    MeasurementWindow window(8);
    window.push(make_reading("s", -0.5));

    ThresholdRule rule(0.0, 1.0);
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::DEGRADED);
}

TEST(ThresholdRule, InvalidReadingReturnsFailed) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 0.5, false));

    ThresholdRule rule(0.0, 1.0);
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::FAILED);
}

// --- ImplausibleValueRule ---

TEST(ImplausibleValueRule, PlausibleValueIsOK) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 50.0));

    ImplausibleValueRule rule(-1.0, 200.0);
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::OK);
}

TEST(ImplausibleValueRule, ImplausibleValueFails) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 999.0));

    ImplausibleValueRule rule(-1.0, 200.0);
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::FAILED);
}

// --- RateOfChangeRule ---

TEST(RateOfChangeRule, StableValueIsOK) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 0.5));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    window.push(make_reading("s", 0.51));

    RateOfChangeRule rule(10.0); // generous limit
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::OK);
}

TEST(RateOfChangeRule, RapidChangeIsDegraded) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 0.1));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    window.push(make_reading("s", 0.95));

    RateOfChangeRule rule(1.0); // 1.0/s limit, actual is ~17/s
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::DEGRADED);
}

TEST(RateOfChangeRule, SingleReadingIsOK) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 0.5));

    RateOfChangeRule rule(1.0);
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::OK);
}

// --- ThresholdRule: Per-Sensor Targeting ---

TEST(ThresholdRule, TargetSensorAppliesOnlyToTarget) {
    MeasurementWindow window(8);
    window.push(make_reading("cpu_load", 0.99));
    window.push(make_reading("battery", 0.99));

    // Rule targets only cpu_load
    ThresholdRule rule(0.0, 0.95, RuleSeverity::DEGRADED, "cpu_load");

    auto cpu_result = rule.evaluate(window, "cpu_load");
    EXPECT_EQ(cpu_result.severity, RuleSeverity::DEGRADED);

    auto battery_result = rule.evaluate(window, "battery");
    EXPECT_EQ(battery_result.severity, RuleSeverity::OK); // skipped
}

TEST(ThresholdRule, BatteryInvertedThreshold) {
    MeasurementWindow window(8);

    // Battery at 100% — should be OK with inverted bounds [0.05, 1.0]
    window.push(make_reading("battery", 1.0));
    ThresholdRule rule(0.05, 1.0, RuleSeverity::DEGRADED, "battery");
    auto result = rule.evaluate(window, "battery");
    EXPECT_EQ(result.severity, RuleSeverity::OK);
}

TEST(ThresholdRule, BatteryLowTriggersDegraded) {
    MeasurementWindow window(8);

    // Battery at 2% — should be DEGRADED with inverted bounds [0.05, 1.0]
    window.push(make_reading("battery", 0.02));
    ThresholdRule rule(0.05, 1.0, RuleSeverity::DEGRADED, "battery");
    auto result = rule.evaluate(window, "battery");
    EXPECT_EQ(result.severity, RuleSeverity::DEGRADED);
}

// --- ThresholdRule: Bounds Map ---

TEST(ThresholdRule, BoundsMapMultipleSensors) {
    MeasurementWindow window(8);
    window.push(make_reading("cpu_load", 0.99));
    window.push(make_reading("memory", 0.5));
    window.push(make_reading("battery", 0.02));

    ThresholdRule rule(std::unordered_map<std::string, ThresholdBounds>{
        {"cpu_load", {0.0, 0.95, RuleSeverity::DEGRADED}},
        {"memory",   {0.0, 0.95, RuleSeverity::DEGRADED}},
        {"battery",  {0.05, 1.0, RuleSeverity::DEGRADED}},
    });

    auto cpu = rule.evaluate(window, "cpu_load");
    EXPECT_EQ(cpu.severity, RuleSeverity::DEGRADED); // 0.99 > 0.95

    auto mem = rule.evaluate(window, "memory");
    EXPECT_EQ(mem.severity, RuleSeverity::OK); // 0.5 within [0, 0.95]

    auto bat = rule.evaluate(window, "battery");
    EXPECT_EQ(bat.severity, RuleSeverity::DEGRADED); // 0.02 < 0.05
}

TEST(ThresholdRule, BoundsMapUnknownSensorIsOK) {
    MeasurementWindow window(8);
    window.push(make_reading("temperature", 99.0));

    ThresholdRule rule(std::unordered_map<std::string, ThresholdBounds>{
        {"cpu_load", {0.0, 0.95, RuleSeverity::DEGRADED}},
    });

    auto result = rule.evaluate(window, "temperature");
    EXPECT_EQ(result.severity, RuleSeverity::OK);
}

TEST(ThresholdRule, BoundsMapInvalidReadingReturnsFailed) {
    MeasurementWindow window(8);
    window.push(make_reading("cpu_load", 0.5, false));

    ThresholdRule rule(std::unordered_map<std::string, ThresholdBounds>{
        {"cpu_load", {0.0, 0.95, RuleSeverity::DEGRADED}},
    });

    auto result = rule.evaluate(window, "cpu_load");
    EXPECT_EQ(result.severity, RuleSeverity::FAILED);
}

TEST(ThresholdRule, BoundsMapEmptyMapAllSensorsOK) {
    MeasurementWindow window(8);
    window.push(make_reading("cpu_load", 0.99));
    window.push(make_reading("memory", 0.99));

    ThresholdRule rule(std::unordered_map<std::string, ThresholdBounds>{});

    auto cpu = rule.evaluate(window, "cpu_load");
    EXPECT_EQ(cpu.severity, RuleSeverity::OK);

    auto mem = rule.evaluate(window, "memory");
    EXPECT_EQ(mem.severity, RuleSeverity::OK);
}

TEST(ThresholdRule, BoundsMapPerSensorSeverity) {
    MeasurementWindow window(8);
    window.push(make_reading("cpu_load", 0.99));
    window.push(make_reading("battery", 0.01));

    ThresholdRule rule(std::unordered_map<std::string, ThresholdBounds>{
        {"cpu_load", {0.0, 0.95, RuleSeverity::DEGRADED}},
        {"battery",  {0.05, 1.0, RuleSeverity::FAILED}},
    });

    auto cpu = rule.evaluate(window, "cpu_load");
    EXPECT_EQ(cpu.severity, RuleSeverity::DEGRADED);

    auto bat = rule.evaluate(window, "battery");
    EXPECT_EQ(bat.severity, RuleSeverity::FAILED);
}

// --- MissingDataRule ---

TEST(MissingDataRule, RecentReadingIsOK) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 0.5));

    MissingDataRule rule(
        std::chrono::milliseconds(5000),
        std::chrono::milliseconds(15000)
    );
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::OK);
}

TEST(MissingDataRule, StaleReadingIsDegraded) {
    MeasurementWindow window(8);

    // Push a reading with a timestamp in the past
    SensorReading r;
    r.sensor_id = "s";
    r.value = 0.5;
    r.valid = true;
    r.timestamp = std::chrono::steady_clock::now() - std::chrono::milliseconds(6000);
    window.push(r);

    MissingDataRule rule(
        std::chrono::milliseconds(5000),
        std::chrono::milliseconds(15000)
    );
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::DEGRADED);
}

TEST(MissingDataRule, VeryStaleReadingIsFailed) {
    MeasurementWindow window(8);

    SensorReading r;
    r.sensor_id = "s";
    r.value = 0.5;
    r.valid = true;
    r.timestamp = std::chrono::steady_clock::now() - std::chrono::milliseconds(20000);
    window.push(r);

    MissingDataRule rule(
        std::chrono::milliseconds(5000),
        std::chrono::milliseconds(15000)
    );
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::FAILED);
}

TEST(MissingDataRule, InvalidReadingIsFailed) {
    MeasurementWindow window(8);
    window.push(make_reading("s", 0.5, false));

    MissingDataRule rule(
        std::chrono::milliseconds(5000),
        std::chrono::milliseconds(15000)
    );
    auto result = rule.evaluate(window, "s");
    EXPECT_EQ(result.severity, RuleSeverity::FAILED);
}
