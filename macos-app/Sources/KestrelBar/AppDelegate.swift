import AppKit

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
    }

    func applicationWillTerminate(_ notification: Notification) {
        coreProcess?.stop()
    }

    // MARK: - Menu

    private func buildMenu() {
        let menu = NSMenu()

        // --- Aggregate status ---
        let dot = statusDot(for: aggregateLabel)
        let stateItem = NSMenuItem(title: "", action: nil, keyEquivalent: "")
        stateItem.attributedTitle = styledTitle(
            "\(dot)  System: \(aggregateLabel)",
            size: 13, weight: .semibold
        )
        stateItem.isEnabled = false
        menu.addItem(stateItem)

        // Severity explanation
        let severityDesc: String
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

        menu.addItem(NSMenuItem.separator())
        menu.addItem(NSMenuItem(title: "Quit Kestrel", action: #selector(quit), keyEquivalent: "q"))

        statusItem.menu = menu
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
        let diagnosis = sensorDiagnosis(sensor: sensor, state: state, value: value, valid: valid, pct: pct, threshold: threshold)
        addInfoBlock(to: menu, label: "Why?", body: diagnosis, bodyColor: barColor)

        // "Check" — troubleshooting tip below
        let tip = sensorTroubleshootTip(sensor: sensor, state: state, pct: pct)
        addInfoBlock(to: menu, label: "Check", body: tip, bodyColor: .labelColor)

        // Clickable actions — always shown so people can jump to the right place
        menu.addItem(NSMenuItem.separator())
        for action in sensorActions(sensor: sensor, state: state, value: value, threshold: threshold) {
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

    private func sensorDiagnosis(sensor: String, state: String, value: Double, valid: Bool, pct: Int, threshold: Double) -> String {
        if state == "UNKNOWN" {
            return "Waiting for the first reading from this sensor."
        }

        if state == "FAILED" {
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

        switch sensor {
        case "cpu_load":
            if state == "OK" {
                if pct < 20 {
                    return "Your Mac is using \(pct)% of its CPU. Mostly idle — running smoothly."
                } else if pct < 50 {
                    return "Your Mac is using \(pct)% of its CPU. Normal activity — nothing unusual."
                } else {
                    return "Your Mac is using \(pct)% of its CPU. Moderate load but still within healthy range."
                }
            } else {
                return "Your Mac is using \(pct)% of its CPU. Something is pushing the processor hard. Check Activity Monitor to find which app is responsible."
            }

        case "memory":
            if state == "OK" {
                if pct < 50 {
                    return "Your Mac is using \(pct)% of its memory. Plenty of room for apps to run."
                } else {
                    return "Your Mac is using \(pct)% of its memory. Healthy — still enough free for normal use."
                }
            } else {
                return "Your Mac is using \(pct)% of its memory. Apps may slow down or swap to disk. Check Activity Monitor for memory-heavy apps."
            }

        case "battery":
            if state == "OK" {
                if pct > 80 {
                    return "Battery is at \(pct)%. Fully healthy — no action needed."
                } else if pct > 30 {
                    return "Battery is at \(pct)%. Charge level is fine for normal use."
                } else {
                    return "Battery is at \(pct)%. Getting lower but still within healthy range. Consider plugging in soon."
                }
            } else {
                return "Battery is critically low at \(pct)%. Your Mac may shut down soon — connect to power now."
            }

        case "storage":
            if state == "OK" {
                if pct < 50 {
                    return "Your disk is \(pct)% full. Plenty of free space available."
                } else {
                    return "Your disk is \(pct)% full. Still enough room, but keep an eye on large files."
                }
            } else {
                return "Your disk is \(pct)% full. Running low on space can cause slowdowns. Try emptying Trash or removing large unused files."
            }

        default:
            if state == "OK" {
                return "Sensor reading \(pct)% — operating normally."
            } else {
                return "Sensor reading \(pct)% — outside expected range. Check system settings."
            }
        }
    }

    private func sensorTroubleshootTip(sensor: String, state: String, pct: Int) -> String {
        switch sensor {
        case "cpu_load":
            if state == "OK" {
                return "Activity Monitor → CPU tab shows what's running and how much each app uses."
            } else {
                return "Activity Monitor → CPU tab → sort by % CPU to find the heavy app. You can quit it from there."
            }

        case "memory":
            if state == "OK" {
                return "Activity Monitor → Memory tab shows usage per app. The Memory Pressure graph at the bottom shows overall health."
            } else {
                return "Activity Monitor → Memory tab → sort by Memory. Close browser tabs and unused apps first — they're usually the biggest consumers."
            }

        case "battery":
            if state == "OK" {
                return "System Settings → Battery shows charge history and battery health. Look for apps using significant energy."
            } else {
                return "Plug in your charger now. System Settings → Battery shows which apps are draining power fastest."
            }

        case "storage":
            if state == "OK" {
                return "System Settings → General → Storage shows what's using disk space and offers cleanup recommendations."
            } else {
                return "System Settings → General → Storage to see what's using space. Quick wins: empty Trash, clear Downloads, delete old iOS backups."
            }

        default:
            return "Check System Settings or Activity Monitor for details on this sensor."
        }
    }

    private struct SensorAction {
        let id: String
        let title: String
    }

    private func sensorActions(sensor: String, state: String, value: Double, threshold: Double) -> [SensorAction] {
        var actions: [SensorAction] = []

        switch sensor {
        case "cpu_load":
            actions.append(SensorAction(id: "open_activity_monitor", title: "Open Activity Monitor"))
        case "memory":
            actions.append(SensorAction(id: "open_activity_monitor", title: "Open Activity Monitor"))
        case "battery":
            actions.append(SensorAction(id: "open_battery_settings", title: "Open Energy Settings"))
        case "storage":
            actions.append(SensorAction(id: "open_storage_settings", title: "Open Storage Settings"))
        default:
            break
        }

        return actions
    }

    @objc private func sensorActionClicked(_ sender: NSMenuItem) {
        guard let actionId = sender.representedObject as? String else { return }

        switch actionId {
        case "open_activity_monitor":
            NSWorkspace.shared.open(URL(fileURLWithPath: "/System/Applications/Utilities/Activity Monitor.app"))
        case "open_battery_settings":
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.preference.battery")!)
        case "open_storage_settings":
            NSWorkspace.shared.open(URL(string: "x-apple.systempreferences:com.apple.settings.Storage")!)
        case "adjust_sensitivity":
            // Rebuild menu with settings focused — user can find it in Settings submenu
            break
        default:
            break
        }
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

        let str = NSMutableAttributedString(
            string: "\(dot)  \(label.padding(toLength: 10, withPad: " ", startingAt: 0))",
            attributes: [.font: NSFont.systemFont(ofSize: 12, weight: .regular)]
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

    @objc private func openProject() {
        NSWorkspace.shared.open(URL(string: "https://github.com/akuligowski9/kestrel")!)
    }

    // MARK: - Status Dots

    private func statusDot(for state: String) -> String {
        switch state {
        case "OK", "Healthy":       return "\u{1F7E2}"
        case "DEGRADED", "Warning": return "\u{1F7E1}"
        case "FAILED", "Alert":     return "\u{1F534}"
        default:                    return "\u{26AA}"
        }
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

    private func startCore() {
        guard let path = findCoreBinary() else {
            print("[KestrelBar] could not find kestrel binary")
            return
        }

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
    }

    private func restartCore() {
        coreProcess?.stop()
        sensorStates = [:]
        degradedSince = [:]
        aggregateLabel = "UNKNOWN"
        startCore()
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
            self?.buildMenu()
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
