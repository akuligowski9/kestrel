#include "engine/engine.h"
#include "fault/fault_injector.h"
#include "sensors/sensor_manager.h"
#include "rules/implausible_value_rule.h"
#include "rules/threshold_rule.h"

#include <gtest/gtest.h>

using namespace kestrel;

// Controllable sensor for integration testing.
class MockSensor : public ISensor {
public:
    explicit MockSensor(std::string id, double initial_value = 0.5)
        : id_(std::move(id)), value_(initial_value) {}

    SensorReading read() override {
        SensorReading r;
        r.sensor_id = id_;
        r.value = value_;
        r.valid = valid_;
        r.timestamp = std::chrono::steady_clock::now();
        return r;
    }

    std::string id() const override { return id_; }

    void set_value(double v) { value_ = v; }
    void set_valid(bool v) { valid_ = v; }

private:
    std::string id_;
    double value_;
    bool valid_ = true;
};

static SensorReading make_reading(const std::string& id, double value,
                                  bool valid = true) {
    SensorReading r;
    r.sensor_id = id;
    r.value = value;
    r.valid = valid;
    r.timestamp = std::chrono::steady_clock::now();
    return r;
}

// --- SensorManager + Engine integration ---

TEST(Integration, SensorManagerFeedsEngine) {
    SensorManager mgr;
    auto* cpu_ptr = new MockSensor("cpu_load", 0.30);
    auto* mem_ptr = new MockSensor("memory", 0.50);
    mgr.register_sensor(std::unique_ptr<ISensor>(cpu_ptr),
                         std::chrono::milliseconds(0));
    mgr.register_sensor(std::unique_ptr<ISensor>(mem_ptr),
                         std::chrono::milliseconds(0));

    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 0.90));

    auto readings = mgr.poll();
    ASSERT_EQ(readings.size(), 2u);

    engine.process(readings);
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);
    EXPECT_EQ(engine.sensor_state("memory"), SystemState::OK);
    EXPECT_EQ(engine.aggregate_state(), SystemState::OK);
}

TEST(Integration, SensorManagerWithDegradedReading) {
    SensorManager mgr;
    auto* sensor = new MockSensor("cpu_load", 0.95);
    mgr.register_sensor(std::unique_ptr<ISensor>(sensor),
                         std::chrono::milliseconds(0));

    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 0.90));

    engine.process(mgr.poll());
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::DEGRADED);
}

// --- FaultInjector + Engine pipeline ---

TEST(Integration, FaultInjectionPipeline) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));
    engine.add_rule(std::make_unique<ImplausibleValueRule>(-1.0, 200.0));

    FaultInjector injector;

    // Step 1: Normal reading -> OK
    engine.process({injector.apply(make_reading("cpu_load", 0.50))});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);

    // Step 2: Inject spike -> DEGRADED (value exceeds threshold)
    injector.inject("cpu_load", FaultType::SPIKE, {1.5, 0, 0});
    engine.process({injector.apply(make_reading("cpu_load", 0.50))});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::DEGRADED);

    // Step 3: Spike is one-shot, next reading is clean -> OK
    engine.process({injector.apply(make_reading("cpu_load", 0.50))});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);
}

TEST(Integration, InterfaceFailureCausesFailed) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    FaultInjector injector;
    injector.inject("battery", FaultType::INTERFACE_FAILURE, {});

    engine.process({injector.apply(make_reading("battery", 0.80))});
    EXPECT_EQ(engine.sensor_state("battery"), SystemState::FAILED);

    // Clear fault -> recovery
    injector.clear("battery");
    engine.process({injector.apply(make_reading("battery", 0.80))});
    EXPECT_EQ(engine.sensor_state("battery"), SystemState::OK);
}

// --- Multi-rule evaluation ---

TEST(Integration, MultipleRulesEvaluatedTogether) {
    Engine engine;
    // ImplausibleValueRule first so it can short-circuit to FAILED
    engine.add_rule(std::make_unique<ImplausibleValueRule>(-1.0, 200.0));
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 0.90));

    // Within threshold and plausible -> OK
    engine.process({make_reading("cpu_load", 0.50)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);

    // Exceeds threshold but still plausible -> DEGRADED
    engine.process({make_reading("cpu_load", 0.95)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::DEGRADED);

    // Implausible value -> FAILED (caught by ImplausibleValueRule before ThresholdRule)
    engine.process({make_reading("cpu_load", 500.0)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::FAILED);
}

// --- Aggregate state with multiple sensors ---

TEST(Integration, AggregateStateTracksWorstAcrossSensors) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 0.90));

    // All OK
    engine.process({make_reading("cpu_load", 0.30)});
    engine.process({make_reading("memory", 0.50)});
    engine.process({make_reading("battery", 0.80)});
    engine.process({make_reading("storage", 0.40)});
    EXPECT_EQ(engine.aggregate_state(), SystemState::OK);

    // One sensor degrades -> aggregate DEGRADED
    engine.process({make_reading("memory", 0.95)});
    EXPECT_EQ(engine.sensor_state("memory"), SystemState::DEGRADED);
    EXPECT_EQ(engine.aggregate_state(), SystemState::DEGRADED);

    // Another sensor fails -> aggregate FAILED
    engine.process({make_reading("cpu_load", 0.0, false)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::FAILED);
    EXPECT_EQ(engine.aggregate_state(), SystemState::FAILED);

    // Recovery of failed sensor -> aggregate back to DEGRADED (memory still degraded)
    engine.process({make_reading("cpu_load", 0.30)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);
    EXPECT_EQ(engine.aggregate_state(), SystemState::DEGRADED);

    // Memory recovers too -> aggregate OK
    engine.process({make_reading("memory", 0.50)});
    EXPECT_EQ(engine.aggregate_state(), SystemState::OK);
}

// --- Transition history ---

TEST(Integration, TransitionHistoryTracksFullLifecycle) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 0.90));

    engine.process({make_reading("cpu_load", 0.50)});  // UNKNOWN -> OK
    engine.process({make_reading("cpu_load", 0.95)});  // OK -> DEGRADED
    engine.process({make_reading("cpu_load", 0.0, false)}); // DEGRADED -> FAILED
    engine.process({make_reading("cpu_load", 0.50)});  // FAILED -> OK

    const auto& transitions = engine.recent_transitions();
    ASSERT_EQ(transitions.size(), 4u);

    EXPECT_EQ(transitions[0].from, SystemState::UNKNOWN);
    EXPECT_EQ(transitions[0].to, SystemState::OK);

    EXPECT_EQ(transitions[1].from, SystemState::OK);
    EXPECT_EQ(transitions[1].to, SystemState::DEGRADED);

    EXPECT_EQ(transitions[2].from, SystemState::DEGRADED);
    EXPECT_EQ(transitions[2].to, SystemState::FAILED);

    EXPECT_EQ(transitions[3].from, SystemState::FAILED);
    EXPECT_EQ(transitions[3].to, SystemState::OK);
}

// --- MissingUpdate fault over multiple cycles ---

TEST(Integration, MissingUpdateCausesTemporaryFailure) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    FaultInjector injector;
    FaultParameters params;
    params.suppress_cycles = 2;
    injector.inject("cpu_load", FaultType::MISSING_UPDATE, params);

    // Normal reading first -> OK
    engine.process({make_reading("cpu_load", 0.50)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);

    // Two suppressed cycles -> FAILED
    engine.process({injector.apply(make_reading("cpu_load", 0.50))});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::FAILED);

    engine.process({injector.apply(make_reading("cpu_load", 0.50))});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::FAILED);

    // Third cycle: fault exhausted, reading passes through -> OK
    engine.process({injector.apply(make_reading("cpu_load", 0.50))});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);
}

// --- Measurement window integration ---

TEST(Integration, EngineWindowStoresReadings) {
    Engine engine(16); // small window
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    for (int i = 0; i < 20; ++i) {
        engine.process({make_reading("cpu_load", 0.1 * (i % 10))});
    }

    auto readings = engine.window().readings_for("cpu_load");
    // Window capacity is 16, so should not exceed it
    EXPECT_LE(readings.size(), 16u);
    EXPECT_GT(readings.size(), 0u);
}

// --- Concurrent sensor degradation and recovery ---

TEST(Integration, IndependentSensorRecovery) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 0.90));

    // Both degrade
    engine.process({make_reading("cpu_load", 0.95)});
    engine.process({make_reading("memory", 0.92)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::DEGRADED);
    EXPECT_EQ(engine.sensor_state("memory"), SystemState::DEGRADED);

    // Only CPU recovers
    engine.process({make_reading("cpu_load", 0.50)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);
    EXPECT_EQ(engine.sensor_state("memory"), SystemState::DEGRADED);
    EXPECT_EQ(engine.aggregate_state(), SystemState::DEGRADED);

    // Memory recovers too
    engine.process({make_reading("memory", 0.40)});
    EXPECT_EQ(engine.sensor_state("memory"), SystemState::OK);
    EXPECT_EQ(engine.aggregate_state(), SystemState::OK);
}

// --- FaultInjector clear_all ---

TEST(Integration, ClearAllFaultsRestoresNormalOperation) {
    FaultInjector injector;
    injector.inject("cpu_load", FaultType::INTERFACE_FAILURE, {});
    injector.inject("memory", FaultType::INVALID_VALUE, {-1.0, 0, 0});
    injector.inject("battery", FaultType::SPIKE, {0.99, 0, 0});

    EXPECT_TRUE(injector.has_fault("cpu_load"));
    EXPECT_TRUE(injector.has_fault("memory"));
    EXPECT_TRUE(injector.has_fault("battery"));

    injector.clear_all();

    EXPECT_FALSE(injector.has_fault("cpu_load"));
    EXPECT_FALSE(injector.has_fault("memory"));
    EXPECT_FALSE(injector.has_fault("battery"));

    // All readings pass through unmodified
    auto r1 = injector.apply(make_reading("cpu_load", 0.50));
    EXPECT_TRUE(r1.valid);
    EXPECT_DOUBLE_EQ(r1.value, 0.50);

    auto r2 = injector.apply(make_reading("memory", 0.60));
    EXPECT_TRUE(r2.valid);
    EXPECT_DOUBLE_EQ(r2.value, 0.60);
}

// --- Edge cases ---

TEST(Integration, UnknownSensorIdReturnsUnknownState) {
    Engine engine;
    EXPECT_EQ(engine.sensor_state("nonexistent"), SystemState::UNKNOWN);
}

TEST(Integration, ProcessEmptyReadingsVector) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    // Should not crash or change state
    engine.process({});
    EXPECT_EQ(engine.aggregate_state(), SystemState::UNKNOWN);
}

TEST(Integration, FaultOnUnregisteredSensorIsIgnored) {
    FaultInjector injector;
    injector.inject("nonexistent", FaultType::SPIKE, {0.99, 0, 0});

    // Applying to a different sensor should pass through
    auto result = injector.apply(make_reading("cpu_load", 0.50));
    EXPECT_DOUBLE_EQ(result.value, 0.50);
    EXPECT_TRUE(result.valid);
}
