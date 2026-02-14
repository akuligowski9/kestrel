add_library(kestrel_core
    sensors/sensor_manager.cpp
    sensors/mac/cpu_load_sensor.cpp
    sensors/mac/battery_sensor.cpp
    sensors/mac/memory_sensor.cpp
    sensors/mac/storage_sensor.cpp
    engine/engine.cpp
    engine/measurement_window.cpp
    rules/threshold_rule.cpp
    rules/missing_data_rule.cpp
    rules/implausible_value_rule.cpp
    rules/rate_of_change_rule.cpp
    fault/fault_injector.cpp
    fault/fault_profile.cpp
    logging/logger.cpp
)

target_include_directories(kestrel_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(kestrel_core PUBLIC nlohmann_json::nlohmann_json)

add_executable(kestrel main.cpp)
target_link_libraries(kestrel PRIVATE kestrel_core)
