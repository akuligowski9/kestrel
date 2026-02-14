#pragma once

#include <chrono>
#include <string>

namespace kestrel {

enum class SystemState {
    OK,
    DEGRADED,
    FAILED,
    UNKNOWN
};

inline const char* to_string(SystemState state) {
    switch (state) {
        case SystemState::OK:       return "OK";
        case SystemState::DEGRADED: return "DEGRADED";
        case SystemState::FAILED:   return "FAILED";
        case SystemState::UNKNOWN:  return "UNKNOWN";
    }
    return "UNKNOWN";
}

struct StateTransition {
    std::string sensor_id;
    SystemState from;
    SystemState to;
    std::string reason;
    std::chrono::steady_clock::time_point timestamp;
};

} // namespace kestrel
