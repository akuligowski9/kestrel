import Foundation

/// Pure diagnostic functions for generating human-readable sensor explanations.
/// Stateless and independently testable — no dependency on UI or AppDelegate.
enum SensorDiagnostics {

    // MARK: - Why?

    static func diagnosis(sensor: String, state: String, value: Double, valid: Bool, pct: Int, threshold: Double) -> String {
        if state == "UNKNOWN" {
            return "Waiting for the first reading from this sensor."
        }

        if state == "FAILED" {
            return failedDiagnosis(sensor: sensor)
        }

        switch sensor {
        case "cpu_load":
            return cpuDiagnosis(state: state, pct: pct)
        case "memory":
            return memoryDiagnosis(state: state, pct: pct)
        case "battery":
            return batteryDiagnosis(state: state, pct: pct)
        case "storage":
            return storageDiagnosis(state: state, pct: pct)
        default:
            return state == "OK"
                ? "Sensor reading \(pct)% — operating normally."
                : "Sensor reading \(pct)% — outside expected range. Check system settings."
        }
    }

    // MARK: - Check

    static func troubleshootTip(sensor: String, state: String) -> String {
        switch sensor {
        case "cpu_load":
            return state == "OK"
                ? "Activity Monitor → CPU tab shows what's running and how much each app uses."
                : "Activity Monitor → CPU tab → sort by % CPU to find the heavy app. You can quit it from there."
        case "memory":
            return state == "OK"
                ? "Activity Monitor → Memory tab shows usage per app. The Memory Pressure graph at the bottom shows overall health."
                : "Activity Monitor → Memory tab → sort by Memory. Close browser tabs and unused apps first — they're usually the biggest consumers."
        case "battery":
            return state == "OK"
                ? "System Settings → Battery shows charge history and battery health. Look for apps using significant energy."
                : "Plug in your charger now. System Settings → Battery shows which apps are draining power fastest."
        case "storage":
            return state == "OK"
                ? "System Settings → General → Storage shows what's using disk space and offers cleanup recommendations."
                : "System Settings → General → Storage to see what's using space. Quick wins: empty Trash, clear Downloads, delete old iOS backups."
        default:
            return "Check System Settings or Activity Monitor for details on this sensor."
        }
    }

    // MARK: - Actions

    struct Action {
        let id: String
        let title: String
    }

    static func actions(sensor: String) -> [Action] {
        switch sensor {
        case "cpu_load":  return [Action(id: "open_activity_monitor", title: "Open Activity Monitor")]
        case "memory":    return [Action(id: "open_activity_monitor", title: "Open Activity Monitor")]
        case "battery":   return [Action(id: "open_battery_settings", title: "Open Energy Settings")]
        case "storage":   return [Action(id: "open_storage_settings", title: "Open Storage Settings")]
        default:          return []
        }
    }

    // MARK: - Private

    private static func failedDiagnosis(sensor: String) -> String {
        switch sensor {
        case "cpu_load":
            return "The CPU sensor stopped responding. This can happen briefly during heavy system load."
        case "memory":
            return "The memory sensor stopped responding. A restart usually resolves this."
        case "battery":
            return "The battery sensor isn't reporting data. This can happen if your Mac can't read battery hardware."
        case "storage":
            return "The disk sensor stopped responding. Check that your disk is accessible."
        default:
            return "This sensor stopped responding. Kestrel will recover when data resumes."
        }
    }

    private static func cpuDiagnosis(state: String, pct: Int) -> String {
        if state == "OK" {
            if pct < 20 {
                return "Your Mac is using \(pct)% of its CPU. Mostly idle — running smoothly."
            } else if pct < 50 {
                return "Your Mac is using \(pct)% of its CPU. Normal activity — nothing unusual."
            } else {
                return "Your Mac is using \(pct)% of its CPU. Moderate load but still within healthy range."
            }
        }
        return "Your Mac is using \(pct)% of its CPU. Something is pushing the processor hard. Check Activity Monitor to find which app is responsible."
    }

    private static func memoryDiagnosis(state: String, pct: Int) -> String {
        if state == "OK" {
            return pct < 50
                ? "Your Mac is using \(pct)% of its memory. Plenty of room for apps to run."
                : "Your Mac is using \(pct)% of its memory. Healthy — still enough free for normal use."
        }
        return "Your Mac is using \(pct)% of its memory. Apps may slow down or swap to disk. Check Activity Monitor for memory-heavy apps."
    }

    private static func batteryDiagnosis(state: String, pct: Int) -> String {
        if state == "OK" {
            if pct > 80 {
                return "Battery is at \(pct)%. Fully healthy — no action needed."
            } else if pct > 30 {
                return "Battery is at \(pct)%. Charge level is fine for normal use."
            } else {
                return "Battery is at \(pct)%. Getting lower but still within healthy range. Consider plugging in soon."
            }
        }
        return "Battery is critically low at \(pct)%. Your Mac may shut down soon — connect to power now."
    }

    private static func storageDiagnosis(state: String, pct: Int) -> String {
        if state == "OK" {
            return pct < 50
                ? "Your disk is \(pct)% full. Plenty of free space available."
                : "Your disk is \(pct)% full. Still enough room, but keep an eye on large files."
        }
        return "Your disk is \(pct)% full. Running low on space can cause slowdowns. Try emptying Trash or removing large unused files."
    }
}
