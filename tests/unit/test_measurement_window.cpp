#include "engine/measurement_window.h"

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

TEST(MeasurementWindow, EmptyWindowReturnsInvalidLatest) {
    MeasurementWindow window(8);
    auto latest = window.latest("cpu_load");
    EXPECT_FALSE(latest.valid);
    EXPECT_EQ(latest.sensor_id, "cpu_load");
}

TEST(MeasurementWindow, PushAndRetrieveLatest) {
    MeasurementWindow window(8);
    window.push(make_reading("cpu_load", 0.5));
    window.push(make_reading("cpu_load", 0.7));

    auto latest = window.latest("cpu_load");
    EXPECT_TRUE(latest.valid);
    EXPECT_DOUBLE_EQ(latest.value, 0.7);
}

TEST(MeasurementWindow, ReadingsForReturnsInOrder) {
    MeasurementWindow window(8);
    window.push(make_reading("mem", 0.3));
    window.push(make_reading("mem", 0.5));
    window.push(make_reading("mem", 0.8));

    auto readings = window.readings_for("mem");
    ASSERT_EQ(readings.size(), 3u);
    EXPECT_DOUBLE_EQ(readings[0].value, 0.3);
    EXPECT_DOUBLE_EQ(readings[1].value, 0.5);
    EXPECT_DOUBLE_EQ(readings[2].value, 0.8);
}

TEST(MeasurementWindow, RespectsBoundedCapacity) {
    MeasurementWindow window(3);
    window.push(make_reading("s", 1.0));
    window.push(make_reading("s", 2.0));
    window.push(make_reading("s", 3.0));
    window.push(make_reading("s", 4.0)); // should evict 1.0

    auto readings = window.readings_for("s");
    ASSERT_EQ(readings.size(), 3u);
    EXPECT_DOUBLE_EQ(readings[0].value, 2.0);
    EXPECT_DOUBLE_EQ(readings[1].value, 3.0);
    EXPECT_DOUBLE_EQ(readings[2].value, 4.0);
}

TEST(MeasurementWindow, SeparatesSensors) {
    MeasurementWindow window(8);
    window.push(make_reading("a", 1.0));
    window.push(make_reading("b", 2.0));

    EXPECT_DOUBLE_EQ(window.latest("a").value, 1.0);
    EXPECT_DOUBLE_EQ(window.latest("b").value, 2.0);
    EXPECT_EQ(window.readings_for("a").size(), 1u);
    EXPECT_EQ(window.readings_for("b").size(), 1u);
}
