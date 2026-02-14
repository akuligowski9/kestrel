import AppKit

class BadgeView: NSView {
    private let color: NSColor
    private let symbol: String

    init(frame: NSRect, color: NSColor, symbol: String) {
        self.color = color
        self.symbol = symbol
        super.init(frame: frame)
        wantsLayer = true
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) not implemented")
    }

    override func draw(_ dirtyRect: NSRect) {
        let path = NSBezierPath(ovalIn: bounds)
        color.setFill()
        path.fill()

        let attrs: [NSAttributedString.Key: Any] = [
            .font: NSFont.systemFont(ofSize: 7, weight: .bold),
            .foregroundColor: NSColor.white,
        ]
        let str = NSAttributedString(string: symbol, attributes: attrs)
        let strSize = str.size()
        let point = NSPoint(
            x: bounds.midX - strSize.width / 2,
            y: bounds.midY - strSize.height / 2
        )
        str.draw(at: point)
    }
}
