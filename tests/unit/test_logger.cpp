#include "logging/logger.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

using namespace kestrel;

namespace {

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        log_path_ = std::filesystem::temp_directory_path() / "kestrel_test_log.jsonl";
        // Ensure clean slate
        std::filesystem::remove(log_path_);
    }

    void TearDown() override {
        std::filesystem::remove(log_path_);
    }

    std::vector<nlohmann::json> read_log_lines() {
        std::vector<nlohmann::json> lines;
        std::ifstream file(log_path_);
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                lines.push_back(nlohmann::json::parse(line));
            }
        }
        return lines;
    }

    std::filesystem::path log_path_;
};

} // namespace

TEST_F(LoggerTest, LogReadingWritesValidJson) {
    {
        Logger logger(log_path_.string());

        SensorReading reading;
        reading.sensor_id = "cpu_load";
        reading.value = 0.75;
        reading.valid = true;
        reading.timestamp = std::chrono::steady_clock::now();

        logger.log_reading(reading);
    } // Logger destructor flushes and closes file

    auto lines = read_log_lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0]["type"], "reading");
    EXPECT_EQ(lines[0]["sensor"], "cpu_load");
    EXPECT_DOUBLE_EQ(lines[0]["value"].get<double>(), 0.75);
    EXPECT_EQ(lines[0]["valid"], true);
    EXPECT_TRUE(lines[0].contains("ts"));
}

TEST_F(LoggerTest, LogTransitionWritesValidJson) {
    {
        Logger logger(log_path_.string());

        StateTransition t;
        t.sensor_id = "memory";
        t.from = SystemState::OK;
        t.to = SystemState::DEGRADED;
        t.reason = "threshold_exceeded";
        t.timestamp = std::chrono::steady_clock::now();

        logger.log_transition(t);
    }

    auto lines = read_log_lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0]["type"], "transition");
    EXPECT_EQ(lines[0]["sensor"], "memory");
    EXPECT_EQ(lines[0]["from"], "OK");
    EXPECT_EQ(lines[0]["to"], "DEGRADED");
    EXPECT_EQ(lines[0]["reason"], "threshold_exceeded");
}

TEST_F(LoggerTest, LogFaultWritesValidJson) {
    {
        Logger logger(log_path_.string());
        logger.log_fault("battery", "SPIKE", 0.99);
    }

    auto lines = read_log_lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0]["type"], "fault");
    EXPECT_EQ(lines[0]["sensor"], "battery");
    EXPECT_EQ(lines[0]["fault_type"], "SPIKE");
    EXPECT_DOUBLE_EQ(lines[0]["injected_value"].get<double>(), 0.99);
}

TEST_F(LoggerTest, LogRuleViolationWritesValidJson) {
    {
        Logger logger(log_path_.string());

        RuleResult result;
        result.rule_name = "ThresholdRule";
        result.sensor_id = "storage";
        result.severity = RuleSeverity::DEGRADED;
        result.message = "value 0.95 exceeds upper bound 0.90";

        logger.log_rule_violation(result);
    }

    auto lines = read_log_lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0]["type"], "rule_violation");
    EXPECT_EQ(lines[0]["rule"], "ThresholdRule");
    EXPECT_EQ(lines[0]["sensor"], "storage");
    EXPECT_EQ(lines[0]["message"], "value 0.95 exceeds upper bound 0.90");
}

TEST_F(LoggerTest, MultipleEntriesAppendToFile) {
    {
        Logger logger(log_path_.string());

        SensorReading r1;
        r1.sensor_id = "cpu_load";
        r1.value = 0.10;
        r1.valid = true;
        r1.timestamp = std::chrono::steady_clock::now();

        SensorReading r2;
        r2.sensor_id = "memory";
        r2.value = 0.80;
        r2.valid = true;
        r2.timestamp = std::chrono::steady_clock::now();

        logger.log_reading(r1);
        logger.log_reading(r2);
    }

    auto lines = read_log_lines();
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0]["sensor"], "cpu_load");
    EXPECT_EQ(lines[1]["sensor"], "memory");
}

TEST_F(LoggerTest, TimestampIsIso8601Format) {
    {
        Logger logger(log_path_.string());

        SensorReading reading;
        reading.sensor_id = "cpu_load";
        reading.value = 0.50;
        reading.valid = true;
        reading.timestamp = std::chrono::steady_clock::now();

        logger.log_reading(reading);
    }

    auto lines = read_log_lines();
    ASSERT_EQ(lines.size(), 1u);

    std::string ts = lines[0]["ts"];
    // ISO 8601 format: YYYY-MM-DDTHH:MM:SSZ
    EXPECT_EQ(ts.length(), 20u);
    EXPECT_EQ(ts[4], '-');
    EXPECT_EQ(ts[7], '-');
    EXPECT_EQ(ts[10], 'T');
    EXPECT_EQ(ts[13], ':');
    EXPECT_EQ(ts[16], ':');
    EXPECT_EQ(ts[19], 'Z');
}

TEST_F(LoggerTest, EmptyPathDoesNotCrash) {
    // Logger with empty path should still work (writes to stdout only)
    Logger logger("");

    SensorReading reading;
    reading.sensor_id = "cpu_load";
    reading.value = 0.50;
    reading.valid = true;
    reading.timestamp = std::chrono::steady_clock::now();

    // Should not throw
    EXPECT_NO_THROW(logger.log_reading(reading));
}

TEST_F(LoggerTest, InvalidReadingLoggedCorrectly) {
    {
        Logger logger(log_path_.string());

        SensorReading reading;
        reading.sensor_id = "cpu_load";
        reading.value = 0.0;
        reading.valid = false;
        reading.timestamp = std::chrono::steady_clock::now();

        logger.log_reading(reading);
    }

    auto lines = read_log_lines();
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0]["valid"], false);
}

TEST_F(LoggerTest, MixedLogTypes) {
    {
        Logger logger(log_path_.string());

        SensorReading reading;
        reading.sensor_id = "cpu_load";
        reading.value = 0.90;
        reading.valid = true;
        reading.timestamp = std::chrono::steady_clock::now();
        logger.log_reading(reading);

        StateTransition t;
        t.sensor_id = "cpu_load";
        t.from = SystemState::OK;
        t.to = SystemState::DEGRADED;
        t.reason = "threshold";
        t.timestamp = std::chrono::steady_clock::now();
        logger.log_transition(t);

        logger.log_fault("cpu_load", "SPIKE", 0.99);

        RuleResult rr;
        rr.rule_name = "ThresholdRule";
        rr.sensor_id = "cpu_load";
        rr.severity = RuleSeverity::DEGRADED;
        rr.message = "above threshold";
        logger.log_rule_violation(rr);
    }

    auto lines = read_log_lines();
    ASSERT_EQ(lines.size(), 4u);
    EXPECT_EQ(lines[0]["type"], "reading");
    EXPECT_EQ(lines[1]["type"], "transition");
    EXPECT_EQ(lines[2]["type"], "fault");
    EXPECT_EQ(lines[3]["type"], "rule_violation");
}
