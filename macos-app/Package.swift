// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "KestrelBar",
    platforms: [.macOS(.v14)],
    targets: [
        .target(
            name: "KestrelBarLib",
            path: "Sources/KestrelBarLib"
        ),
        .executableTarget(
            name: "KestrelBar",
            dependencies: ["KestrelBarLib"],
            path: "Sources/KestrelBar"
        ),
        .testTarget(
            name: "KestrelBarLibTests",
            dependencies: ["KestrelBarLib"],
            path: "Tests/KestrelBarLibTests"
        ),
    ]
)
