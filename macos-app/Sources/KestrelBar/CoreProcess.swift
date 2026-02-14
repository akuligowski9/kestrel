import Foundation

class CoreProcess {
    private let binaryPath: String
    private let faultConfigPath: String?
    private let threshold: Double
    private var process: Process?
    private var outputPipe: Pipe?

    var onLine: ((String) -> Void)?

    init(binaryPath: String, faultConfigPath: String? = nil, threshold: Double = 0.95) {
        self.binaryPath = binaryPath
        self.faultConfigPath = faultConfigPath
        self.threshold = threshold
    }

    func start() {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: binaryPath)

        var args = ["--log", "/dev/null", "--threshold", String(threshold)]
        if let faultPath = faultConfigPath {
            args += ["--fault", faultPath]
        }
        process.arguments = args

        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = FileHandle.nullDevice

        self.process = process
        self.outputPipe = pipe

        // Read stdout line by line on a background queue
        pipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            guard !data.isEmpty else { return }

            if let output = String(data: data, encoding: .utf8) {
                let lines = output.components(separatedBy: .newlines)
                for line in lines where !line.isEmpty {
                    self?.onLine?(line)
                }
            }
        }

        process.terminationHandler = { [weak self] _ in
            self?.outputPipe?.fileHandleForReading.readabilityHandler = nil
        }

        do {
            try process.run()
            print("[KestrelBar] core process started (pid: \(process.processIdentifier))")
        } catch {
            print("[KestrelBar] failed to start core: \(error)")
        }
    }

    func stop() {
        if let process = process, process.isRunning {
            process.interrupt() // sends SIGINT, same as Ctrl+C
            process.waitUntilExit()
            print("[KestrelBar] core process stopped")
        }
        outputPipe?.fileHandleForReading.readabilityHandler = nil
    }
}
