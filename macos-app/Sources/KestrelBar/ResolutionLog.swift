import Foundation

struct ResolutionEntry: Codable {
    let timestamp: String
    let sensor: String
    let previousState: String
    let durationSeconds: Double
}

class ResolutionLog {
    private let fileURL: URL
    private(set) var entries: [ResolutionEntry] = []

    init() {
        let appSupport = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
        let kestrelDir = appSupport.appendingPathComponent("Kestrel")
        try? FileManager.default.createDirectory(at: kestrelDir, withIntermediateDirectories: true)
        fileURL = kestrelDir.appendingPathComponent("resolutions.jsonl")
        load()
    }

    func add(sensor: String, previousState: String, duration: Double) {
        let formatter = ISO8601DateFormatter()
        let entry = ResolutionEntry(
            timestamp: formatter.string(from: Date()),
            sensor: sensor,
            previousState: previousState,
            durationSeconds: duration
        )
        entries.append(entry)

        // Append to file
        if let data = try? JSONEncoder().encode(entry),
           let line = String(data: data, encoding: .utf8) {
            let handle: FileHandle
            if FileManager.default.fileExists(atPath: fileURL.path) {
                handle = try! FileHandle(forWritingTo: fileURL)
                handle.seekToEndOfFile()
            } else {
                FileManager.default.createFile(atPath: fileURL.path, contents: nil)
                handle = try! FileHandle(forWritingTo: fileURL)
            }
            handle.write((line + "\n").data(using: .utf8)!)
            handle.closeFile()
        }
    }

    func recent(_ count: Int = 10) -> [ResolutionEntry] {
        Array(entries.suffix(count).reversed())
    }

    private func load() {
        guard let data = try? String(contentsOf: fileURL, encoding: .utf8) else { return }
        let decoder = JSONDecoder()
        for line in data.components(separatedBy: .newlines) where !line.isEmpty {
            if let lineData = line.data(using: .utf8),
               let entry = try? decoder.decode(ResolutionEntry.self, from: lineData) {
                entries.append(entry)
            }
        }
    }
}
