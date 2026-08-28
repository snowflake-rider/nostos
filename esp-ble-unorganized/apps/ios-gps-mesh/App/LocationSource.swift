import CoreLocation
import UIKit
import GPSCore

@MainActor final class LocationSource: NSObject, @preconcurrency CLLocationManagerDelegate {
    private let manager = CLLocationManager()
    private var updating = false
    var onFixes: (([LocationFix]) -> Void)?
    var onIssue: ((String) -> Void)?
    var onPermissionRevoked: (() -> Void)?
    var onAuthorizationChange: (() -> Void)?
    var authorizationStatus: CLAuthorizationStatus { manager.authorizationStatus }
    var accuracyAuthorization: CLAccuracyAuthorization { manager.accuracyAuthorization }

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBest
        manager.distanceFilter = kCLDistanceFilterNone
        manager.activityType = .fitness
        manager.pausesLocationUpdatesAutomatically = false
        manager.showsBackgroundLocationIndicator = true
    }
    // Permission is independent of Mesh. The owner decides when to collect locations.
    func requestPermission() {
        guard UIApplication.shared.applicationState == .active else { return }
        if manager.authorizationStatus == .notDetermined {
            manager.requestWhenInUseAuthorization()
        }
    }
    @discardableResult
    func updateDemand(viewing: Bool, sharing: Bool) -> Bool {
        let authorized = manager.authorizationStatus == .authorizedWhenInUse || manager.authorizationStatus == .authorizedAlways
        guard (viewing || sharing), authorized else { stop(); return false }
        // A background session must first be started while the app is visible.
        guard updating || UIApplication.shared.applicationState == .active else { return false }
        manager.allowsBackgroundLocationUpdates = sharing
        if !updating {
            updating = true
            manager.startUpdatingLocation()
        }
        return true
    }
    func stop() {
        if updating { manager.stopUpdatingLocation() }
        updating = false
        manager.allowsBackgroundLocationUpdates = false
    }
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        onAuthorizationChange?()
    }
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard updating else { return }
        onFixes?(locations.map {
            LocationFix(latitude: $0.coordinate.latitude, longitude: $0.coordinate.longitude,
                        accuracy: $0.horizontalAccuracy, measuredAt: $0.timestamp.timeIntervalSince1970)
        })
    }
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        guard updating else { return }
        onIssue?("위치를 가져오지 못했습니다. 실외에서 확인하세요.")
        if (error as? CLError)?.code == .denied { stop(); onPermissionRevoked?() }
    }
}
