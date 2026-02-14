include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(kestrel_tests
    unit/test_measurement_window.cpp
    unit/test_rules.cpp
    unit/test_engine.cpp
    fault/test_fault_injection.cpp
)

target_link_libraries(kestrel_tests PRIVATE kestrel_core GTest::gtest_main)
gtest_discover_tests(kestrel_tests)
