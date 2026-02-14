import Testing
@testable import KestrelBarLib

@Suite("SensorDiagnostics")
struct SensorDiagnosticsTests {

    // MARK: - diagnosis() — UNKNOWN / FAILED states (all sensors)

    @Test func diagnosisUnknownState() {
        let result = SensorDiagnostics.diagnosis(sensor: "cpu_load", state: "UNKNOWN", value: 0.0, valid: false, pct: 0, threshold: 0.95)
        #expect(result.contains("Waiting"))
    }

    @Test func diagnosisFailedCPU() {
        let result = SensorDiagnostics.diagnosis(sensor: "cpu_load", state: "FAILED", value: 0.0, valid: false, pct: 0, threshold: 0.95)
        #expect(result.contains("CPU sensor stopped"))
    }

    @Test func diagnosisFailedMemory() {
        let result = SensorDiagnostics.diagnosis(sensor: "memory", state: "FAILED", value: 0.0, valid: false, pct: 0, threshold: 0.95)
        #expect(result.contains("memory sensor stopped"))
    }

    @Test func diagnosisFailedBattery() {
        let result = SensorDiagnostics.diagnosis(sensor: "battery", state: "FAILED", value: 0.0, valid: false, pct: 0, threshold: 0.95)
        #expect(result.contains("battery sensor"))
    }

    @Test func diagnosisFailedStorage() {
        let result = SensorDiagnostics.diagnosis(sensor: "storage", state: "FAILED", value: 0.0, valid: false, pct: 0, threshold: 0.95)
        #expect(result.contains("disk sensor stopped"))
    }

    @Test func diagnosisFailedUnknownSensor() {
        let result = SensorDiagnostics.diagnosis(sensor: "pressure", state: "FAILED", value: 0.0, valid: false, pct: 0, threshold: 0.95)
        #expect(result.contains("stopped responding"))
    }

    // MARK: - diagnosis() — CPU boundary values

    @Test func diagnosisCPUOKLow() {
        // pct=15 < 20 → "Mostly idle"
        let result = SensorDiagnostics.diagnosis(sensor: "cpu_load", state: "OK", value: 0.15, valid: true, pct: 15, threshold: 0.95)
        #expect(result.contains("Mostly idle"))
    }

    @Test func diagnosisCPUOKMid() {
        // pct=35, 20..49 → "Normal activity"
        let result = SensorDiagnostics.diagnosis(sensor: "cpu_load", state: "OK", value: 0.35, valid: true, pct: 35, threshold: 0.95)
        #expect(result.contains("Normal activity"))
    }

    @Test func diagnosisCPUOKHigh() {
        // pct=60, >=50 → "Moderate load"
        let result = SensorDiagnostics.diagnosis(sensor: "cpu_load", state: "OK", value: 0.60, valid: true, pct: 60, threshold: 0.95)
        #expect(result.contains("Moderate load"))
    }

    @Test func diagnosisCPUDegraded() {
        let result = SensorDiagnostics.diagnosis(sensor: "cpu_load", state: "DEGRADED", value: 0.97, valid: true, pct: 97, threshold: 0.95)
        #expect(result.contains("pushing the processor hard"))
    }

    // MARK: - diagnosis() — Memory boundary values

    @Test func diagnosisMemoryOKLow() {
        // pct=30 < 50 → "Plenty of room"
        let result = SensorDiagnostics.diagnosis(sensor: "memory", state: "OK", value: 0.30, valid: true, pct: 30, threshold: 0.95)
        #expect(result.contains("Plenty of room"))
    }

    @Test func diagnosisMemoryOKHigh() {
        // pct=65, >=50 → "Healthy"
        let result = SensorDiagnostics.diagnosis(sensor: "memory", state: "OK", value: 0.65, valid: true, pct: 65, threshold: 0.95)
        #expect(result.contains("Healthy"))
    }

    @Test func diagnosisMemoryDegraded() {
        let result = SensorDiagnostics.diagnosis(sensor: "memory", state: "DEGRADED", value: 0.97, valid: true, pct: 97, threshold: 0.95)
        #expect(result.contains("slow down"))
    }

    // MARK: - diagnosis() — Battery boundary values

    @Test func diagnosisBatteryOKFull() {
        // pct=90 > 80 → "Fully healthy"
        let result = SensorDiagnostics.diagnosis(sensor: "battery", state: "OK", value: 0.90, valid: true, pct: 90, threshold: 0.95)
        #expect(result.contains("Fully healthy"))
    }

    @Test func diagnosisBatteryOKMid() {
        // pct=45, 31..80 → "fine for normal use"
        let result = SensorDiagnostics.diagnosis(sensor: "battery", state: "OK", value: 0.45, valid: true, pct: 45, threshold: 0.95)
        #expect(result.contains("fine for normal use"))
    }

    @Test func diagnosisBatteryOKLow() {
        // pct=20, <=30 → "Consider plugging in"
        let result = SensorDiagnostics.diagnosis(sensor: "battery", state: "OK", value: 0.20, valid: true, pct: 20, threshold: 0.95)
        #expect(result.contains("plugging in"))
    }

    @Test func diagnosisBatteryDegraded() {
        // pct=3 → "critically low"
        let result = SensorDiagnostics.diagnosis(sensor: "battery", state: "DEGRADED", value: 0.03, valid: true, pct: 3, threshold: 0.95)
        #expect(result.contains("critically low"))
    }

    // MARK: - diagnosis() — Storage boundary values

    @Test func diagnosisStorageOKLow() {
        // pct=40 < 50 → "Plenty of free space"
        let result = SensorDiagnostics.diagnosis(sensor: "storage", state: "OK", value: 0.40, valid: true, pct: 40, threshold: 0.95)
        #expect(result.contains("Plenty of free space"))
    }

    @Test func diagnosisStorageOKHigh() {
        // pct=65, >=50 → "keep an eye on"
        let result = SensorDiagnostics.diagnosis(sensor: "storage", state: "OK", value: 0.65, valid: true, pct: 65, threshold: 0.95)
        #expect(result.contains("keep an eye on"))
    }

    @Test func diagnosisStorageDegraded() {
        let result = SensorDiagnostics.diagnosis(sensor: "storage", state: "DEGRADED", value: 0.97, valid: true, pct: 97, threshold: 0.95)
        #expect(result.contains("Running low"))
    }

    // MARK: - diagnosis() — Unknown sensor

    @Test func diagnosisUnknownSensorOK() {
        let result = SensorDiagnostics.diagnosis(sensor: "pressure", state: "OK", value: 0.5, valid: true, pct: 50, threshold: 0.95)
        #expect(result.contains("operating normally"))
    }

    @Test func diagnosisUnknownSensorDegraded() {
        let result = SensorDiagnostics.diagnosis(sensor: "pressure", state: "DEGRADED", value: 0.5, valid: true, pct: 50, threshold: 0.95)
        #expect(result.contains("outside expected range"))
    }

    // MARK: - troubleshootTip()

    @Test func troubleshootTipCPUOK() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "cpu_load", state: "OK")
        #expect(result.contains("CPU tab"))
        #expect(!result.contains("sort by"))
    }

    @Test func troubleshootTipCPUDegraded() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "cpu_load", state: "DEGRADED")
        #expect(result.contains("sort by % CPU"))
    }

    @Test func troubleshootTipMemoryOK() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "memory", state: "OK")
        #expect(result.contains("Memory tab"))
        #expect(result.contains("Memory Pressure"))
    }

    @Test func troubleshootTipMemoryDegraded() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "memory", state: "DEGRADED")
        #expect(result.contains("sort by Memory"))
    }

    @Test func troubleshootTipBatteryOK() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "battery", state: "OK")
        #expect(result.contains("Battery"))
    }

    @Test func troubleshootTipBatteryDegraded() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "battery", state: "DEGRADED")
        #expect(result.contains("Plug in"))
    }

    @Test func troubleshootTipStorageOK() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "storage", state: "OK")
        #expect(result.contains("Storage"))
    }

    @Test func troubleshootTipStorageDegraded() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "storage", state: "DEGRADED")
        #expect(result.contains("empty Trash"))
    }

    @Test func troubleshootTipUnknownSensor() {
        let result = SensorDiagnostics.troubleshootTip(sensor: "pressure", state: "OK")
        #expect(result.contains("System Settings"))
    }

    // MARK: - actions()

    @Test func actionsCPU() {
        let actions = SensorDiagnostics.actions(sensor: "cpu_load")
        #expect(actions.count == 1)
        #expect(actions[0].id == "open_activity_monitor")
    }

    @Test func actionsMemory() {
        let actions = SensorDiagnostics.actions(sensor: "memory")
        #expect(actions.count == 1)
        #expect(actions[0].id == "open_activity_monitor")
    }

    @Test func actionsBattery() {
        let actions = SensorDiagnostics.actions(sensor: "battery")
        #expect(actions.count == 1)
        #expect(actions[0].id == "open_battery_settings")
    }

    @Test func actionsStorage() {
        let actions = SensorDiagnostics.actions(sensor: "storage")
        #expect(actions.count == 1)
        #expect(actions[0].id == "open_storage_settings")
    }

    @Test func actionsUnknownSensor() {
        let actions = SensorDiagnostics.actions(sensor: "pressure")
        #expect(actions.isEmpty)
    }
}
