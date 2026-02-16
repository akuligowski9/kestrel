import AppKit
import KestrelBarLib
import ServiceManagement
import UserNotifications

class AppDelegate: NSObject, NSApplicationDelegate {
    private var statusItem: NSStatusItem!
    private var coreProcess: CoreProcess?
    private var badgeView: NSView?

    private var sensorStates: [String: SensorDisplayState] = [:]
    private var aggregateLabel: String = "UNKNOWN"

    private let sensorOrder = ["cpu_load", "memory", "battery", "storage"]

    // Track when sensors entered a bad state (for resolution duration)
    private var degradedSince: [String: Date] = [:]
    private var resolutionLog = ResolutionLog()

    // Settings
    private var currentSensitivity: String = "Normal"
    private var disabledSensors: Set<String> = []

    // Incremental menu refs
    private var aggregateHeaderItem: NSMenuItem?
    private var severityDescItem: NSMenuItem?
    private var sensorRowItems: [String: NSMenuItem] = [:]

    // Watchdog
    private var watchdogTimer: Timer?
    private var lastDataTimestamp: Date = Date()
    private var restartAttempts: Int = 0
    private let maxRestartAttempts: Int = 5
    private let watchdogInterval: TimeInterval = 5.0
    private let staleDataTimeout: TimeInterval = 15.0
    private var backoffDelay: TimeInterval = 1.0

    struct SensorDisplayState {
        var value: Double = 0.0
        var state: String = "UNKNOWN"
        var valid: Bool = true
    }

    private let sensorInfo: [String: (label: String, source: String, poll: String, unit: String)] = [
        "cpu_load": ("CPU", "Mach kernel statistics", "1s", "%"),
        "memory":   ("Memory", "vm_statistics64", "2s", "%"),
        "battery":  ("Battery", "pmset", "5s", "%"),
        "storage":  ("Storage", "statfs", "10s", "%"),
    ]

    private let sensitivityOptions: [(label: String, threshold: Double, description: String)] = [
        ("Relaxed",  0.98, "Triggers only at extreme values. Fewer alerts."),
        ("Normal",   0.95, "Balanced thresholds for typical monitoring."),
        ("Strict",   0.85, "Lower thresholds, earlier detection. More alerts."),
    ]

    func applicationDidFinishLaunching(_ notification: Notification) {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        updateIcon()
        buildMenu()
        startCore()
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert]) { _, _ in }
    }

    func applicationWillTerminate(_ notification: Notification) {
        stopWatchdog()
        coreProcess?.stop()
    }

    // MARK: - Menu

    private func buildMenu() {
        let menu = NSMenu()

        // Clear incremental refs
        sensorRowItems = [:]

        // --- Aggregate status ---
        let dot = statusDot(for: aggregateLabel)
        let stateItem = NSMenuItem(title: "", action: nil, keyEquivalent: "")
        stateItem.attributedTitle = styledTitle(
            "\(dot)  System: \(aggregateLabel)",
            size: 13, weight: .semibold
        )
        stateItem.isEnabled = false
        menu.addItem(stateItem)
        self.aggregateHeaderItem = stateItem

        // Severity explanation
        let severityDesc: String
        if let error = coreError {
            severityDesc = error
        } else {
            switch aggregateLabel {
            case "Healthy":
                severityDesc = "All sensors reporting within expected bounds."
            case "Warning":
                severityDesc = "One sensor is outside expected bounds or unresponsive."
            case "Alert":
                severityDesc = "Multiple sensors are outside expected bounds."
            default:
                severityDesc = "Waiting for sensor data..."
            }
        }
        let descItem = NSMenuItem(title: "", action: nil, keyEquivalent: "")
        descItem.attributedTitle = NSAttributedString(
            string: "     \(severityDesc)",
            attributes: [
                .font: NSFont.systemFont(ofSize: 11, weight: .regular),
                .foregroundColor: NSColor.secondaryLabelColor,
            ]
        )
        descItem.isEnabled = false
        menu.addItem(descItem)
        self.severityDescItem = descItem

        menu.addItem(NSMenuItem.separator())

        // --- Sensor rows ---
        for sensor in sensorOrder {
            let info = sensorInfo[sensor]!

            if disabledSensors.contains(sensor) {
                let item = NSMenuItem(title: "", action: nil, keyEquivalent: "")
                item.attributedTitle = NSAttributedString(
                    string: "\u{26AA}  \(info.label.padding(toLength: 10, withPad: " ", startingAt: 0))disabled",
                    attributes: [
                        .font: NSFont.systemFont(ofSize: 12),
                        .foregroundColor: NSColor.tertiaryLabelColor,
                    ]
                )
                item.isEnabled = false
                menu.addItem(item)
                continue
            }

            let display = sensorStates[sensor]
            let value = display?.value ?? 0.0
            let state = display?.state ?? "—"
            let dot = statusDot(for: state)
            let pct = Int(value * 100)

            let item = NSMenuItem(title: "", action: nil, keyEquivalent: "")
            item.attributedTitle = sensorRowTitle(
                dot: dot, label: info.label, pct: pct, state: state
            )

            // Submenu
            let sub = NSMenu()
            addSensorDetail(to: sub, sensor: sensor, display: display, info: info)
            item.submenu = sub

            menu.addItem(item)
            sensorRowItems[sensor] = item
        }

        menu.addItem(NSMenuItem.separator())

        // --- Resolution History ---
        let historyItem = NSMenuItem(title: "Resolution History", action: nil, keyEquivalent: "")
        historyItem.attributedTitle = styledTitle("Resolution History", size: 12, weight: .medium)
        let historySub = NSMenu()
        let recent = resolutionLog.recent(8)
        if recent.isEmpty {
            let emptyItem = NSMenuItem(title: "No resolutions yet", action: nil, keyEquivalent: "")
            emptyItem.isEnabled = false
            emptyItem.attributedTitle = NSAttributedString(
                string: "No resolutions yet",
                attributes: [
                    .font: NSFont.systemFont(ofSize: 11),
                    .foregroundColor: NSColor.tertiaryLabelColor,
                ]
            )
            historySub.addItem(emptyItem)
        } else {
            for entry in recent {
                let sensorLabel = sensorInfo[entry.sensor]?.label ?? entry.sensor
                let duration = formatDuration(entry.durationSeconds)
                let title = "\u{2705}  \(sensorLabel) resolved (\(entry.previousState) \u{2192} OK, \(duration))"

                let histEntry = NSMenuItem(title: "", action: nil, keyEquivalent: "")
                histEntry.isEnabled = false

                let ts = formatTimestamp(entry.timestamp)

                let attrStr = NSMutableAttributedString(
                    string: title,
                    attributes: [.font: NSFont.systemFont(ofSize: 11, weight: .regular)]
                )
                attrStr.append(NSAttributedString(
                    string: "\n     \(ts)",
                    attributes: [
                        .font: NSFont.systemFont(ofSize: 10),
                        .foregroundColor: NSColor.tertiaryLabelColor,
                    ]
                ))
                histEntry.attributedTitle = attrStr
                historySub.addItem(histEntry)
            }
        }
        historyItem.submenu = historySub
        menu.addItem(historyItem)

        menu.addItem(NSMenuItem.separator())

        // --- Settings ---
        let settingsItem = NSMenuItem(title: "Settings", action: nil, keyEquivalent: "")
        settingsItem.attributedTitle = styledTitle("Settings", size: 12, weight: .medium)
        let settingsSub = NSMenu()

        // Sensitivity presets
        let sensitivityHeader = NSMenuItem(title: "Sensitivity", action: nil, keyEquivalent: "")
        sensitivityHeader.isEnabled = false
        sensitivityHeader.attributedTitle = NSAttributedString(
            string: "Sensitivity",
            attributes: [
                .font: NSFont.systemFont(ofSize: 11, weight: .semibold),
                .foregroundColor: NSColor.secondaryLabelColor,
            ]
        )
        settingsSub.addItem(sensitivityHeader)

        for option in sensitivityOptions {
            let check = option.label == currentSensitivity ? "\u{2713} " : "   "
            let item = NSMenuItem(
                title: "\(check)\(option.label) (\(option.threshold))",
                action: #selector(changeSensitivity(_:)),
                keyEquivalent: ""
            )
            item.target = self
            item.representedObject = option.label

            let attrStr = NSMutableAttributedString(
                string: "\(check)\(option.label) (\(option.threshold))",
                attributes: [.font: NSFont.systemFont(ofSize: 12, weight: option.label == currentSensitivity ? .semibold : .regular)]
            )
            attrStr.append(NSAttributedString(
                string: "\n     \(option.description)",
                attributes: [
                    .font: NSFont.systemFont(ofSize: 10),
                    .foregroundColor: NSColor.tertiaryLabelColor,
                ]
            ))
            item.attributedTitle = attrStr
            settingsSub.addItem(item)
        }

        settingsSub.addItem(NSMenuItem.separator())

        // Sensor toggles
        let sensorsHeader = NSMenuItem(title: "Sensors", action: nil, keyEquivalent: "")
        sensorsHeader.isEnabled = false
        sensorsHeader.attributedTitle = NSAttributedString(
            string: "Sensors",
            attributes: [
                .font: NSFont.systemFont(ofSize: 11, weight: .semibold),
                .foregroundColor: NSColor.secondaryLabelColor,
            ]
        )
        settingsSub.addItem(sensorsHeader)

        for sensor in sensorOrder {
            let info = sensorInfo[sensor]!
            let enabled = !disabledSensors.contains(sensor)
            let check = enabled ? "\u{2713} " : "   "
            let item = NSMenuItem(
                title: "\(check)\(info.label)",
                action: #selector(toggleSensor(_:)),
                keyEquivalent: ""
            )
            item.target = self
            item.representedObject = sensor
            item.attributedTitle = NSAttributedString(
                string: "\(check)\(info.label)",
                attributes: [.font: NSFont.systemFont(ofSize: 12, weight: enabled ? .regular : .light)]
            )
            settingsSub.addItem(item)
        }

        settingsSub.addItem(NSMenuItem.separator())

        // Launch at Login
        let launchAtLogin = SMAppService.mainApp.status == .enabled
        let launchCheck = launchAtLogin ? "\u{2713} " : "   "
        let launchItem = NSMenuItem(
            title: "\(launchCheck)Launch at Login",
            action: #selector(toggleLaunchAtLogin),
            keyEquivalent: ""
        )
        launchItem.target = self
        launchItem.attributedTitle = NSAttributedString(
            string: "\(launchCheck)Launch at Login",
            attributes: [.font: NSFont.systemFont(ofSize: 12, weight: launchAtLogin ? .semibold : .regular)]
        )
        settingsSub.addItem(launchItem)

        settingsSub.addItem(NSMenuItem.separator())

        // Project link
        let linkItem = NSMenuItem(
            title: "About Kestrel...",
            action: #selector(openProject),
            keyEquivalent: ""
        )
        linkItem.target = self
        let linkAttr = NSMutableAttributedString(
            string: "About Kestrel...",
            attributes: [.font: NSFont.systemFont(ofSize: 12)]
        )
        linkAttr.append(NSAttributedString(
            string: "\n     github.com/akuligowski9/kestrel",
            attributes: [
                .font: NSFont.systemFont(ofSize: 10),
                .foregroundColor: NSColor.tertiaryLabelColor,
            ]
        ))
        linkItem.attributedTitle = linkAttr
        settingsSub.addItem(linkItem)

        settingsItem.submenu = settingsSub
        menu.addItem(settingsItem)

        // --- Export ---
        let exportItem = NSMenuItem(title: "Export Session Log...", action: #selector(exportLog), keyEquivalent: "e")
        exportItem.target = self
        menu.addItem(exportItem)

        menu.addItem(NSMenuItem.separator())
        menu.addItem(NSMenuItem(title: "Quit Kestrel", action: #selector(quit), keyEquivalent: "q"))

        statusItem.menu = menu
    }

    private func updateMenuInPlace() {
        guard let headerItem = aggregateHeaderItem,
              let descItem = severityDescItem else {
            buildMenu()
            return
        }

        // Update aggregate header
        let dot = statusDot(for: aggregateLabel)
        headerItem.attributedTitle = styledTitle(
            "\(dot)  System: \(aggregateLabel)",
            size: 13, weight: .semibold
        )

        // Update severity description
        let severityDesc: String
        if let error = coreError {
            severityDesc = error
        } else {
            switch aggregateLabel {
            case "Healthy":
                severityDesc = "All sensors reporting within expected bounds."
            case "Warning":
                severityDesc = "One sensor is outside expected bounds or unresponsive."
            case "Alert":
                severityDesc = "Multiple sensors are outside expected bounds."
            default:
                severityDesc = "Waiting for sensor data..."
            }
        }
        descItem.attributedTitle = NSAttributedString(
            string: "     \(severityDesc)",
            attributes: [
                .font: NSFont.systemFont(ofSize: 11, weight: .regular),
                .foregroundColor: NSColor.secondaryLabelColor,
            ]
        )

        // Update each sensor row in place
        for sensor in sensorOrder {
            guard let item = sensorRowItems[sensor],
                  let info = sensorInfo[sensor] else { continue }

            let display = sensorStates[sensor]
            let value = display?.value ?? 0.0
            let state = display?.state ?? "—"
            let sensorDot = statusDot(for: state)
            let pct = Int(value * 100)

            item.attributedTitle = sensorRowTitle(
                dot: sensorDot, label: info.label, pct: pct, state: state
            )

            // Rebuild submenu
            let sub = NSMenu()
            addSensorDetail(to: sub, sensor: sensor, display: display, info: info)
            item.submenu = sub
        }
    }

    // MARK: - Sensor Submenu

    private func addSensorDetail(to menu: NSMenu, sensor: String, display: SensorDisplayState?, info: (label: String, source: String, poll: String, unit: String)) {
        let value = display?.value ?? 0.0
        let state = display?.state ?? "UNKNOWN"
        let valid = display?.valid ?? false
        let pct = Int(value * 100)

        // Progress bar
        let barWidth = 20
        let filled = Int(Double(barWidth) * min(value, 1.0))
        let empty = barWidth - filled
        let bar = String(repeating: "\u{2588}", count: filled) + String(repeating: "\u{2591}", count: empty)

        let barColor: NSColor
        switch state {
        case "OK": barColor = .systemGreen
        case "DEGRADED": barColor = .systemYellow
        case "FAILED": barColor = .systemRed
        default: barColor = .systemGray
        }

        let barItem = NSMenuItem(title: "", action: nil, keyEquivalent: "")
        let barStr = NSMutableAttributedString(
            string: " \(bar) ",
            attributes: [
                .font: NSFont.monospacedSystemFont(ofSize: 11, weight: .regular),
                .foregroundColor: barColor,
            ]
        )
        barStr.append(NSAttributedString(
            string: " \(pct)\(info.unit)  \(statusDot(for: state)) \(state)",
            attributes: [.font: NSFont.systemFont(ofSize: 12, weight: .medium)]
        ))
        barItem.attributedTitle = barStr
        barItem.isEnabled = false
        menu.addItem(barItem)

        menu.addItem(NSMenuItem.separator())

        // "Why?" — label on its own, explanation below
        let threshold = currentThreshold()
        let diagnosis = SensorDiagnostics.diagnosis(sensor: sensor, state: state, value: value, valid: valid, pct: pct, threshold: threshold)
        addInfoBlock(to: menu, label: "Why?", body: diagnosis, bodyColor: barColor)

        // "Check" — troubleshooting tip below
        let tip = SensorDiagnostics.troubleshootTip(sensor: sensor, state: state)
        addInfoBlock(to: menu, label: "Check", body: tip, bodyColor: .labelColor)

        // Clickable actions — always shown so people can jump to the right place
        menu.addItem(NSMenuItem.separator())
        for action in SensorDiagnostics.actions(sensor: sensor) {
            let actionItem = NSMenuItem(
                title: action.title,
                action: #selector(sensorActionClicked(_:)),
                keyEquivalent: ""
            )
            actionItem.target = self
            actionItem.representedObject = action.id
            actionItem.attributedTitle = NSAttributedString(
                string: "  \(action.title)",
                attributes: [.font: NSFont.systemFont(ofSize: 12)]
            )
            menu.addItem(actionItem)
        }

        menu.addItem(NSMenuItem.separator())

        // Compact metadata footer
        let validStr = valid ? "valid" : "invalid"
        let meta = "via \(info.source) · every \(info.poll) · \(validStr)"
        let metaItem = NSMenuItem(title: "", action: nil, keyEquivalent: "")
        metaItem.attributedTitle = NSAttributedString(
            string: "  \(meta)",
            attributes: [
                .font: NSFont.systemFont(ofSize: 10),
                .foregroundColor: NSColor.tertiaryLabelColor,
            ]
        )
        metaItem.isEnabled = false
        menu.addItem(metaItem)
    }

    private func addInfoBlock(to menu: NSMenu, label: String, body: String, bodyColor: NSColor) {
        let item = NSMenuItem(title: "", action: nil, keyEquivalent: "")

        let str = NSMutableAttributedString(
            string: "  \(label)\n",
            attributes: [
                .font: NSFont.systemFont(ofSize: 10, weight: .semibold),
                .foregroundColor: NSColor.tertiaryLabelColor,
            ]
        )
        str.append(NSAttributedString(
            string: "  \(body)",
            attributes: [
                .font: NSFont.systemFont(ofSize: 12),
                .foregroundColor: bodyColor,
            ]
        ))

        item.attributedTitle = str
        item.isEnabled = false
        menu.addItem(item)
    }

    @objc private func sensorActionClicked(_ sender: NSMenuItem) {
        guard let actionId = sender.representedObject as? String else { return }

        switch actionId {
        case "open_activity_monitor":
            openAppByBundleId("com.apple.ActivityMonitor")
        case "open_battery_settings":
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.battery")!)
        case "open_storage_settings":
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.settings.Storage")!)
        default:
            break
        }
    }

    private func openAppByBundleId(_ bundleId: String) {
        guard let url = NSWorkspace.shared.urlForApplication(withBundleIdentifier: bundleId) else { return }
        NSWorkspace.shared.openApplication(at: url, configuration: NSWorkspace.OpenConfiguration())
    }

    // MARK: - Sensor Row

    private func sensorRowTitle(dot: String, label: String, pct: Int, state: String) -> NSAttributedString {
        let miniBarWidth = 8
        let filled = Int(Double(miniBarWidth) * min(Double(pct) / 100.0, 1.0))
        let empty = miniBarWidth - filled
        let miniBar = String(repeating: "\u{2588}", count: filled) + String(repeating: "\u{2591}", count: empty)

        let barColor: NSColor
        switch state {
        case "OK": barColor = .systemGreen
        case "DEGRADED": barColor = .systemYellow
        case "FAILED": barColor = .systemRed
        default: barColor = .systemGray
        }

        let tabStop = NSTextTab(textAlignment: .left, location: 100)
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.tabStops = [tabStop]

        let str = NSMutableAttributedString(
            string: "\(dot)  \(label)\t",
            attributes: [
                .font: NSFont.systemFont(ofSize: 12, weight: .regular),
                .paragraphStyle: paragraphStyle,
            ]
        )
        str.append(NSAttributedString(
            string: miniBar,
            attributes: [
                .font: NSFont.monospacedSystemFont(ofSize: 9, weight: .regular),
                .foregroundColor: barColor,
            ]
        ))
        str.append(NSAttributedString(
            string: "  \(String(format: "%3d", pct))%   \(state)",
            attributes: [.font: NSFont.monospacedSystemFont(ofSize: 11, weight: .regular)]
        ))
        return str
    }

    // MARK: - Actions

    @objc private func quit() {
        coreProcess?.stop()
        NSApp.terminate(nil)
    }

    @objc private func changeSensitivity(_ sender: NSMenuItem) {
        guard let label = sender.representedObject as? String else { return }
        currentSensitivity = label
        restartCore()
        buildMenu()
    }

    @objc private func toggleSensor(_ sender: NSMenuItem) {
        guard let sensor = sender.representedObject as? String else { return }
        if disabledSensors.contains(sensor) {
            disabledSensors.remove(sensor)
        } else {
            disabledSensors.insert(sensor)
            sensorStates.removeValue(forKey: sensor)
            degradedSince.removeValue(forKey: sensor)
        }
        recomputeAggregate()
        updateIcon()
        buildMenu()
    }

    @objc private func toggleLaunchAtLogin() {
        do {
            if SMAppService.mainApp.status == .enabled {
                try SMAppService.mainApp.unregister()
            } else {
                try SMAppService.mainApp.register()
            }
        } catch {
            print("[KestrelBar] launch at login toggle failed: \(error)")
        }
        buildMenu()
    }

    @objc private func openProject() {
        NSWorkspace.shared.open(URL(string: "https://github.com/akuligowski9/kestrel")!)
    }

    // MARK: - Status Dots

    private func statusDot(for state: String) -> String {
        switch state {
        case "OK", "Healthy":        return "\u{1F7E2}"
        case "DEGRADED", "Warning":  return "\u{1F7E1}"
        case "FAILED", "Alert":      return "\u{1F534}"
        case "Error":                return "\u{1F534}"
        default:                     return "\u{26AA}"
        }
    }

    private func sendNotification(sensor: String, from: String, to: String) {
        let label = sensorInfo[sensor]?.label ?? sensor
        let content = UNMutableNotificationContent()
        content.title = "Kestrel: \(label) \(to)"
        content.body = "\(label) changed from \(from) to \(to)."

        let request = UNNotificationRequest(
            identifier: "kestrel-\(sensor)-\(Date().timeIntervalSince1970)",
            content: content,
            trigger: nil
        )
        UNUserNotificationCenter.current().add(request)
    }

    @objc private func exportLog() {
        let panel = NSSavePanel()
        panel.nameFieldStringValue = "kestrel-session.jsonl"
        panel.allowedContentTypes = [.json]

        guard panel.runModal() == .OK, let url = panel.url else { return }

        let lines = sensorStates.compactMap { sensor, state -> String? in
            guard let data = try? JSONSerialization.data(withJSONObject: [
                "sensor": sensor,
                "value": state.value,
                "state": state.state,
                "valid": state.valid,
                "exported": ISO8601DateFormatter().string(from: Date()),
            ]) else { return nil }
            return String(data: data, encoding: .utf8)
        }

        let output = lines.joined(separator: "\n") + "\n"
        try? output.write(to: url, atomically: true, encoding: .utf8)
    }

    // MARK: - Icon

    private func updateIcon() {
        guard let button = statusItem.button else { return }

        let birdConfig = NSImage.SymbolConfiguration(pointSize: 16, weight: .medium)
        if let bird = NSImage(systemSymbolName: "bird", accessibilityDescription: "Kestrel")?
            .withSymbolConfiguration(birdConfig) {
            button.image = bird
            button.image?.isTemplate = true
        }

        badgeView?.removeFromSuperview()
        badgeView = nil

        if aggregateLabel == "Warning" || aggregateLabel == "Alert" {
            let badgeSize: CGFloat = 10
            let badge = BadgeView(
                frame: NSRect(
                    x: button.bounds.width - badgeSize - 1,
                    y: 1,
                    width: badgeSize,
                    height: badgeSize
                ),
                color: aggregateLabel == "Warning" ? .systemYellow : .systemRed,
                symbol: aggregateLabel == "Warning" ? "!" : "\u{00D7}"
            )
            button.addSubview(badge)
            badgeView = badge
        }
    }

    // MARK: - Helpers

    private func styledTitle(_ text: String, size: CGFloat, weight: NSFont.Weight) -> NSAttributedString {
        NSAttributedString(string: text, attributes: [.font: NSFont.systemFont(ofSize: size, weight: weight)])
    }

    private func formatDuration(_ seconds: Double) -> String {
        if seconds < 60 { return "\(Int(seconds))s" }
        if seconds < 3600 { return "\(Int(seconds / 60))m \(Int(seconds.truncatingRemainder(dividingBy: 60)))s" }
        return "\(Int(seconds / 3600))h \(Int((seconds / 60).truncatingRemainder(dividingBy: 60)))m"
    }

    private func formatTimestamp(_ iso: String) -> String {
        let formatter = ISO8601DateFormatter()
        guard let date = formatter.date(from: iso) else { return iso }
        let display = DateFormatter()
        display.dateFormat = "MMM d, h:mm:ss a"
        return display.string(from: date)
    }

    private func currentThreshold() -> Double {
        sensitivityOptions.first(where: { $0.label == currentSensitivity })?.threshold ?? 0.95
    }

    // MARK: - Core Process

    private var coreError: String? = nil

    private func startCore() {
        guard let path = findCoreBinary() else {
            print("[KestrelBar] could not find kestrel binary")
            coreError = "Core binary not found. Reinstall Kestrel or check build."
            aggregateLabel = "Error"
            buildMenu()
            return
        }
        coreError = nil

        let args = ProcessInfo.processInfo.arguments
        var faultPath: String? = nil
        if let idx = args.firstIndex(of: "--fault-config"), idx + 1 < args.count {
            faultPath = args[idx + 1]
        }

        let threshold = currentThreshold()
        print("[KestrelBar] launching core: \(path) (threshold: \(threshold))")

        coreProcess = CoreProcess(
            binaryPath: path,
            faultConfigPath: faultPath,
            threshold: threshold
        )
        coreProcess?.onLine = { [weak self] line in
            self?.handleLine(line)
        }
        coreProcess?.start()
        startWatchdog()
    }

    private func restartCore() {
        stopWatchdog()
        coreProcess?.stop()
        sensorStates = [:]
        degradedSince = [:]
        aggregateLabel = "UNKNOWN"
        lastDataTimestamp = Date()
        startCore()
    }

    // MARK: - Watchdog

    private func startWatchdog() {
        stopWatchdog()
        watchdogTimer = Timer.scheduledTimer(withTimeInterval: watchdogInterval, repeats: true) { [weak self] _ in
            self?.checkCoreHealth()
        }
    }

    private func stopWatchdog() {
        watchdogTimer?.invalidate()
        watchdogTimer = nil
    }

    private func checkCoreHealth() {
        if let core = coreProcess, !core.isRunning {
            handleCoreFailure(reason: "Core process exited unexpectedly")
            return
        }

        let staleness = Date().timeIntervalSince(lastDataTimestamp)
        if staleness > staleDataTimeout {
            handleCoreFailure(reason: "No data received for \(Int(staleness))s")
        }
    }

    private func handleCoreFailure(reason: String) {
        stopWatchdog()
        restartAttempts += 1
        print("[KestrelBar] core failure: \(reason) (attempt \(restartAttempts)/\(maxRestartAttempts))")

        if restartAttempts > maxRestartAttempts {
            coreError = "Core process failed after \(maxRestartAttempts) restart attempts. Restart Kestrel manually."
            aggregateLabel = "Error"
            DispatchQueue.main.async { [weak self] in
                self?.updateIcon()
                self?.buildMenu()
            }
            return
        }

        coreError = "Core restarting (attempt \(restartAttempts)/\(maxRestartAttempts))..."
        DispatchQueue.main.async { [weak self] in
            self?.updateIcon()
            self?.buildMenu()
        }

        let delay = min(backoffDelay, 30.0)
        backoffDelay = min(backoffDelay * 2, 30.0)

        DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
            guard let self = self else { return }
            self.coreProcess?.stop()
            self.lastDataTimestamp = Date()
            self.startCore()
        }
    }

    private func findCoreBinary() -> String? {
        let args = ProcessInfo.processInfo.arguments
        if let idx = args.firstIndex(of: "--core-path"), idx + 1 < args.count {
            let path = args[idx + 1]
            if FileManager.default.isExecutableFile(atPath: path) {
                return path
            }
        }

        if let bundlePath = Bundle.main.executableURL?.deletingLastPathComponent().appendingPathComponent("kestrel").path,
           FileManager.default.isExecutableFile(atPath: bundlePath) {
            return bundlePath
        }

        let candidates = [
            "build/core/kestrel",
            "../build/core/kestrel",
        ]

        for candidate in candidates {
            let url = URL(fileURLWithPath: candidate, relativeTo: URL(fileURLWithPath: FileManager.default.currentDirectoryPath))
            if FileManager.default.isExecutableFile(atPath: url.path) {
                return url.path
            }
        }

        return nil
    }

    // MARK: - JSONL Parsing

    private func handleLine(_ line: String) {
        guard line.hasPrefix("{"), let data = line.data(using: .utf8) else { return }

        guard let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let type = json["type"] as? String else { return }

        lastDataTimestamp = Date()
        if restartAttempts > 0 {
            restartAttempts = 0
            backoffDelay = 1.0
            coreError = nil
        }

        switch type {
        case "reading":
            guard let sensor = json["sensor"] as? String,
                  let value = json["value"] as? Double,
                  let valid = json["valid"] as? Bool else { return }

            var display = sensorStates[sensor] ?? SensorDisplayState()
            display.value = value
            display.valid = valid
            sensorStates[sensor] = display

        case "transition":
            guard let sensor = json["sensor"] as? String,
                  let to = json["to"] as? String,
                  let from = json["from"] as? String else { return }

            var display = sensorStates[sensor] ?? SensorDisplayState()
            display.state = to
            sensorStates[sensor] = display

            // Notify on state changes
            sendNotification(sensor: sensor, from: from, to: to)

            // Track resolution timing
            if to != "OK" && degradedSince[sensor] == nil {
                degradedSince[sensor] = Date()
            } else if to == "OK", let since = degradedSince[sensor] {
                let duration = Date().timeIntervalSince(since)
                resolutionLog.add(sensor: sensor, previousState: from, duration: duration)
                degradedSince.removeValue(forKey: sensor)
            }

            recomputeAggregate()

        default:
            return
        }

        DispatchQueue.main.async { [weak self] in
            self?.updateIcon()
            self?.updateMenuInPlace()
        }
    }

    private func recomputeAggregate() {
        let activeSensors = sensorStates.filter { !disabledSensors.contains($0.key) }

        if activeSensors.isEmpty {
            aggregateLabel = "UNKNOWN"
            return
        }

        var notOkCount = 0
        for (_, display) in activeSensors {
            if display.state != "OK" {
                notOkCount += 1
            }
        }

        if notOkCount == 0 {
            aggregateLabel = "Healthy"
        } else if notOkCount == 1 {
            aggregateLabel = "Warning"
        } else {
            aggregateLabel = "Alert"
        }
    }
}
