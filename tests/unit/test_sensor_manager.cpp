#include "sensors/sensor_manager.h"

#include <gtest/gtest.h>

using namespace kestrel;

// Stub sensor that returns a configurable reading.
class StubSensor : public ISensor {
public:
    StubSensor(std::string sensor_id, double value, bool valid = true)
        : sensor_id_(std::move(sensor_id)), value_(value), valid_(valid) {}

    SensorReading read() override {
        ++read_count_;
        SensorReading r;
        r.sensor_id = sensor_id_;
        r.value = value_;
        r.valid = valid_;
        r.timestamp = std::chrono::steady_clock::now();
        return r;
    }

    std::string id() const override { return sensor_id_; }

    void set_value(double v) { value_ = v; }
    void set_valid(bool v) { valid_ = v; }
    int read_count() const { return read_count_; }

private:
    std::string sensor_id_;
    double value_;
    bool valid_;
    int read_count_ = 0;
};

TEST(SensorManager, PollEmptyReturnsNothing) {
    SensorManager mgr;
    auto readings = mgr.poll();
    EXPECT_TRUE(readings.empty());
}

TEST(SensorManager, FirstPollAlwaysFires) {
    SensorManager mgr;
    mgr.register_sensor(
        std::make_unique<StubSensor>("cpu_load", 0.42),
        std::chrono::milliseconds(5000));

    auto readings = mgr.poll();
    ASSERT_EQ(readings.size(), 1u);
    EXPECT_EQ(readings[0].sensor_id, "cpu_load");
    EXPECT_DOUBLE_EQ(readings[0].value, 0.42);
    EXPECT_TRUE(readings[0].valid);
}

TEST(SensorManager, SecondPollRespectsInterval) {
    SensorManager mgr;
    mgr.register_sensor(
        std::make_unique<StubSensor>("memory", 0.65),
        std::chrono::milliseconds(60000)); // 60s interval

    // First poll fires immediately
    auto r1 = mgr.poll();
    ASSERT_EQ(r1.size(), 1u);

    // Immediate second poll should NOT fire (interval not elapsed)
    auto r2 = mgr.poll();
    EXPECT_TRUE(r2.empty());
}

TEST(SensorManager, MultipleSensorsPolledIndependently) {
    SensorManager mgr;
    mgr.register_sensor(
        std::make_unique<StubSensor>("cpu_load", 0.30),
        std::chrono::milliseconds(60000));
    mgr.register_sensor(
        std::make_unique<StubSensor>("memory", 0.70),
        std::chrono::milliseconds(60000));

    // Both fire on first poll
    auto readings = mgr.poll();
    ASSERT_EQ(readings.size(), 2u);

    // Verify both sensor IDs present
    std::set<std::string> ids;
    for (const auto& r : readings) {
        ids.insert(r.sensor_id);
    }
    EXPECT_TRUE(ids.count("cpu_load"));
    EXPECT_TRUE(ids.count("memory"));
}

TEST(SensorManager, InvalidReadingsAreReturned) {
    SensorManager mgr;
    mgr.register_sensor(
        std::make_unique<StubSensor>("battery", 0.0, false),
        std::chrono::milliseconds(1000));

    auto readings = mgr.poll();
    ASSERT_EQ(readings.size(), 1u);
    EXPECT_FALSE(readings[0].valid);
    EXPECT_EQ(readings[0].sensor_id, "battery");
}

TEST(SensorManager, ReadingTimestampIsPopulated) {
    SensorManager mgr;
    auto before = std::chrono::steady_clock::now();

    mgr.register_sensor(
        std::make_unique<StubSensor>("storage", 0.55),
        std::chrono::milliseconds(1000));

    auto readings = mgr.poll();
    auto after = std::chrono::steady_clock::now();

    ASSERT_EQ(readings.size(), 1u);
    EXPECT_GE(readings[0].timestamp, before);
    EXPECT_LE(readings[0].timestamp, after);
}

TEST(SensorManager, ZeroIntervalAlwaysPolls) {
    SensorManager mgr;
    mgr.register_sensor(
        std::make_unique<StubSensor>("cpu_load", 0.10),
        std::chrono::milliseconds(0));

    auto r1 = mgr.poll();
    ASSERT_EQ(r1.size(), 1u);

    // Even immediate second poll should fire with 0ms interval
    auto r2 = mgr.poll();
    ASSERT_EQ(r2.size(), 1u);
}

TEST(SensorManager, SensorIdMatchesRegistration) {
    SensorManager mgr;
    mgr.register_sensor(
        std::make_unique<StubSensor>("custom_sensor_123", 0.99),
        std::chrono::milliseconds(1000));

    auto readings = mgr.poll();
    ASSERT_EQ(readings.size(), 1u);
    EXPECT_EQ(readings[0].sensor_id, "custom_sensor_123");
}
