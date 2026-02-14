#include "engine/engine.h"
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

TEST(Engine, InitialStateIsUnknown) {
    Engine engine;
    EXPECT_EQ(engine.aggregate_state(), SystemState::UNKNOWN);
}

TEST(Engine, ValidReadingTransitionsToOK) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    engine.process({make_reading("cpu_load", 0.5)});
    EXPECT_EQ(engine.sensor_state("cpu_load"), SystemState::OK);
}

TEST(Engine, OutOfBoundsTransitionsToDegraded) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    engine.process({make_reading("s", 1.5)});
    EXPECT_EQ(engine.sensor_state("s"), SystemState::DEGRADED);
}

TEST(Engine, InvalidReadingTransitionsToFailed) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    engine.process({make_reading("s", 0.0, false)});
    EXPECT_EQ(engine.sensor_state("s"), SystemState::FAILED);
}

TEST(Engine, RecoveryFromDegradedToOK) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    engine.process({make_reading("s", 1.5)});
    EXPECT_EQ(engine.sensor_state("s"), SystemState::DEGRADED);

    engine.process({make_reading("s", 0.5)});
    EXPECT_EQ(engine.sensor_state("s"), SystemState::OK);
}

TEST(Engine, AggregateStateReflectsWorst) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    engine.process({make_reading("a", 0.5)}); // OK
    engine.process({make_reading("b", 1.5)}); // DEGRADED

    EXPECT_EQ(engine.sensor_state("a"), SystemState::OK);
    EXPECT_EQ(engine.sensor_state("b"), SystemState::DEGRADED);
    EXPECT_EQ(engine.aggregate_state(), SystemState::DEGRADED);
}

TEST(Engine, TransitionsAreRecorded) {
    Engine engine;
    engine.add_rule(std::make_unique<ThresholdRule>(0.0, 1.0));

    engine.process({make_reading("s", 0.5)});  // UNKNOWN -> OK
    engine.process({make_reading("s", 1.5)});  // OK -> DEGRADED

    const auto& transitions = engine.recent_transitions();
    ASSERT_EQ(transitions.size(), 2u);
    EXPECT_EQ(transitions[0].from, SystemState::UNKNOWN);
    EXPECT_EQ(transitions[0].to, SystemState::OK);
    EXPECT_EQ(transitions[1].from, SystemState::OK);
    EXPECT_EQ(transitions[1].to, SystemState::DEGRADED);
}
