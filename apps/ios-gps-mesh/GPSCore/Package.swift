// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "GPSCore",
    platforms: [.macOS(.v13), .iOS(.v17)],
    products: [.library(name: "GPSCore", targets: ["GPSCore"])],
    targets: [.target(name: "GPSCore"), .testTarget(name: "GPSCoreTests", dependencies: ["GPSCore"])]
)
