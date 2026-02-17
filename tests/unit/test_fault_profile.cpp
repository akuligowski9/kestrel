#include "fault/fault_profile.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace kestrel;

namespace {

class FaultProfileTest : public ::testing::Test {
protected:
    void SetUp() override {
        profile_path_ = std::filesystem::temp_directory_path() / "kestrel_test_fault_profile.json";
    }

    void TearDown() override {
        std::filesystem::remove(profile_path_);
    }

    void write_profile(const std::string& json_content) {
        std::ofstream file(profile_path_);
        file << json_content;
    }

    std::filesystem::path profile_path_;
};

} // namespace

TEST_F(FaultProfileTest, LoadsSingleFault) {
    write_profile(R"({
        "faults": [
            {
                "sensor_id": "cpu_load",
                "type": "Spike",
                "value": 0.99,
                "trigger_after_s": 5.0,
                "duration_s": 2.0
            }
        ]
    })");

    auto configs = FaultProfile::load(profile_path_.string());
    ASSERT_EQ(configs.size(), 1u);
    EXPECT_EQ(configs[0].sensor_id, "cpu_load");
    EXPECT_EQ(configs[0].type, FaultType::SPIKE);
    EXPECT_DOUBLE_EQ(configs[0].params.injected_value, 0.99);
    EXPECT_DOUBLE_EQ(configs[0].trigger_after_s, 5.0);
    EXPECT_DOUBLE_EQ(configs[0].duration_s, 2.0);
}

TEST_F(FaultProfileTest, LoadsMultipleFaults) {
    write_profile(R"({
        "faults": [
            {
                "sensor_id": "cpu_load",
                "type": "Spike",
                "value": 0.95
            },
            {
                "sensor_id": "memory",
                "type": "InvalidValue",
                "value": -1.0
            },
            {
                "sensor_id": "battery",
                "type": "InterfaceFailure"
            }
        ]
    })");

    auto configs = FaultProfile::load(profile_path_.string());
    ASSERT_EQ(configs.size(), 3u);

    EXPECT_EQ(configs[0].sensor_id, "cpu_load");
    EXPECT_EQ(configs[0].type, FaultType::SPIKE);

    EXPECT_EQ(configs[1].sensor_id, "memory");
    EXPECT_EQ(configs[1].type, FaultType::INVALID_VALUE);
    EXPECT_DOUBLE_EQ(configs[1].params.injected_value, -1.0);

    EXPECT_EQ(configs[2].sensor_id, "battery");
    EXPECT_EQ(configs[2].type, FaultType::INTERFACE_FAILURE);
}

TEST_F(FaultProfileTest, DefaultValuesWhenOptionalFieldsMissing) {
    write_profile(R"({
        "faults": [
            {
                "sensor_id": "storage",
                "type": "MissingUpdate"
            }
        ]
    })");

    auto configs = FaultProfile::load(profile_path_.string());
    ASSERT_EQ(configs.size(), 1u);
    EXPECT_DOUBLE_EQ(configs[0].trigger_after_s, 0.0);
    EXPECT_DOUBLE_EQ(configs[0].duration_s, 0.0);
    EXPECT_DOUBLE_EQ(configs[0].params.injected_value, 0.0);
    EXPECT_EQ(configs[0].params.suppress_cycles, 0);
    EXPECT_EQ(configs[0].params.delay_ms, 0);
}

TEST_F(FaultProfileTest, MissingUpdateWithSuppressCycles) {
    write_profile(R"({
        "faults": [
            {
                "sensor_id": "cpu_load",
                "type": "MissingUpdate",
                "suppress_cycles": 5
            }
        ]
    })");

    auto configs = FaultProfile::load(profile_path_.string());
    ASSERT_EQ(configs.size(), 1u);
    EXPECT_EQ(configs[0].type, FaultType::MISSING_UPDATE);
    EXPECT_EQ(configs[0].params.suppress_cycles, 5);
}

TEST_F(FaultProfileTest, DelayedReadingWithDelayMs) {
    write_profile(R"({
        "faults": [
            {
                "sensor_id": "memory",
                "type": "DelayedReading",
                "delay_ms": 500
            }
        ]
    })");

    auto configs = FaultProfile::load(profile_path_.string());
    ASSERT_EQ(configs.size(), 1u);
    EXPECT_EQ(configs[0].type, FaultType::DELAYED_READING);
    EXPECT_EQ(configs[0].params.delay_ms, 500);
}

TEST_F(FaultProfileTest, EmptyFaultsArray) {
    write_profile(R"({ "faults": [] })");

    auto configs = FaultProfile::load(profile_path_.string());
    EXPECT_TRUE(configs.empty());
}

TEST_F(FaultProfileTest, UnknownFaultTypeThrows) {
    write_profile(R"({
        "faults": [
            {
                "sensor_id": "cpu_load",
                "type": "NonexistentType"
            }
        ]
    })");

    EXPECT_THROW(FaultProfile::load(profile_path_.string()), std::runtime_error);
}

TEST_F(FaultProfileTest, MissingFileThrows) {
    EXPECT_THROW(
        FaultProfile::load("/tmp/nonexistent_kestrel_profile_12345.json"),
        std::runtime_error);
}

TEST_F(FaultProfileTest, InitialRuntimeStateIsClean) {
    write_profile(R"({
        "faults": [
            {
                "sensor_id": "cpu_load",
                "type": "Spike",
                "value": 0.99
            }
        ]
    })");

    auto configs = FaultProfile::load(profile_path_.string());
    ASSERT_EQ(configs.size(), 1u);
    EXPECT_FALSE(configs[0].triggered);
    EXPECT_FALSE(configs[0].cleared);
    EXPECT_DOUBLE_EQ(configs[0].injected_at_s, 0.0);
}

TEST_F(FaultProfileTest, AllFaultTypesLoadCorrectly) {
    write_profile(R"({
        "faults": [
            { "sensor_id": "s1", "type": "Spike", "value": 0.99 },
            { "sensor_id": "s2", "type": "InvalidValue", "value": -5.0 },
            { "sensor_id": "s3", "type": "MissingUpdate", "suppress_cycles": 3 },
            { "sensor_id": "s4", "type": "DelayedReading", "delay_ms": 100 },
            { "sensor_id": "s5", "type": "InterfaceFailure" }
        ]
    })");

    auto configs = FaultProfile::load(profile_path_.string());
    ASSERT_EQ(configs.size(), 5u);
    EXPECT_EQ(configs[0].type, FaultType::SPIKE);
    EXPECT_EQ(configs[1].type, FaultType::INVALID_VALUE);
    EXPECT_EQ(configs[2].type, FaultType::MISSING_UPDATE);
    EXPECT_EQ(configs[3].type, FaultType::DELAYED_READING);
    EXPECT_EQ(configs[4].type, FaultType::INTERFACE_FAILURE);
}
