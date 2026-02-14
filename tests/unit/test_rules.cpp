#include "engine/measurement_window.h"
#include "rules/implausible_value_rule.h"
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
