// swift-tools-version: 6.0
import PackageDescription

// Compile-only verification fallback when Xcode's iOS device platform is missing.
// This does NOT produce an installable .app; GPSMesh.xcodeproj is the app project.
let package = Package(
    name: "GPSMeshCompileCheck",
    platforms: [.iOS("26.0")],
    products: [.library(name: "GPSMeshCompileCheck", targets: ["GPSMeshCompileCheck"])],
    dependencies: [
        .package(path: "GPSCore"),
        .package(url: "https://github.com/nordicsemi/IOS-nRF-Mesh-Library.git", exact: "4.8.0")
    ],
    targets: [.target(name: "GPSMeshCompileCheck", dependencies: [
        .product(name: "GPSCore", package: "GPSCore"),
        .product(name: "NordicMesh", package: "IOS-nRF-Mesh-Library")
    ], path: "App", exclude: ["Info.plist"])],
    swiftLanguageModes: [.v5]
)
