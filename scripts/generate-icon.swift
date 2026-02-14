import AppKit

// Generate Kestrel app icon from SF Symbol
let iconsetPath = "/tmp/Kestrel.iconset"
let fm = FileManager.default
try? fm.removeItem(atPath: iconsetPath)
try! fm.createDirectory(atPath: iconsetPath, withIntermediateDirectories: true)

let sizes: [(String, CGFloat)] = [
    ("icon_16x16.png", 16),
    ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32),
    ("icon_32x32@2x.png", 64),
    ("icon_128x128.png", 128),
    ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256),
    ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512),
    ("icon_512x512@2x.png", 1024),
]

for (filename, size) in sizes {
    let image = NSImage(size: NSSize(width: size, height: size))
    image.lockFocus()

    // Background: rounded rect with gradient
    let bgRect = NSRect(x: 0, y: 0, width: size, height: size)
    let cornerRadius = size * 0.22
    let bgPath = NSBezierPath(roundedRect: bgRect, xRadius: cornerRadius, yRadius: cornerRadius)

    // Dark teal/slate gradient
    let gradient = NSGradient(
        starting: NSColor(red: 0.12, green: 0.16, blue: 0.22, alpha: 1.0),
        ending: NSColor(red: 0.18, green: 0.25, blue: 0.32, alpha: 1.0)
    )!
    gradient.draw(in: bgPath, angle: -90)

    // Draw bird SF Symbol
    let symbolSize = size * 0.55
    let config = NSImage.SymbolConfiguration(pointSize: symbolSize, weight: .thin)
    if let bird = NSImage(systemSymbolName: "bird", accessibilityDescription: nil)?
        .withSymbolConfiguration(config) {

        let tinted = NSImage(size: bird.size)
        tinted.lockFocus()
        NSColor(red: 0.75, green: 0.82, blue: 0.88, alpha: 1.0).set()
        bird.draw(in: NSRect(origin: .zero, size: bird.size),
                  from: .zero, operation: .sourceOver, fraction: 1.0)
        NSColor(red: 0.75, green: 0.82, blue: 0.88, alpha: 1.0).set()
        NSRect(origin: .zero, size: bird.size).fill(using: .sourceAtop)
        tinted.unlockFocus()

        let birdRect = NSRect(
            x: (size - tinted.size.width) / 2,
            y: (size - tinted.size.height) / 2 + size * 0.02,
            width: tinted.size.width,
            height: tinted.size.height
        )
        tinted.draw(in: birdRect)
    }

    image.unlockFocus()

    // Save as PNG
    guard let tiffData = image.tiffRepresentation,
          let bitmap = NSBitmapImageRep(data: tiffData),
          let pngData = bitmap.representation(using: .png, properties: [:]) else {
        print("Failed to create \(filename)")
        continue
    }

    let filePath = (iconsetPath as NSString).appendingPathComponent(filename)
    try! pngData.write(to: URL(fileURLWithPath: filePath))
    print("Created \(filename) (\(Int(size))x\(Int(size)))")
}

print("Iconset created at \(iconsetPath)")
