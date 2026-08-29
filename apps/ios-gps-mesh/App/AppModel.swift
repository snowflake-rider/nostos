import Foundation
import Observation
import CoreLocation
import UIKit
import GPSCore

@MainActor @Observable final class AppModel {
    let mesh = MeshTransport()
    private(set) var sharing = false
    private(set) var locationAuthorization: CLAuthorizationStatus = .notDetermined
    private(set) var locationAccuracy: CLAccuracyAuthorization = .reducedAccuracy
    private(set) var receivingLocation = false
    private(set) var locationIssue: String?
    var testMode = false
    private(set) var latest: LocationFix?
    private(set) var submitted = 0
    private(set) var completed = 0
    private(set) var lastSequence: UInt32?
    private(set) var sessionID: UInt32?
    private(set) var lastTransmission: Date?
    var issue: String?
    @ObservationIgnored private let location = LocationSource()
    @ObservationIgnored private var session = ShareSession()
    @ObservationIgnored private var testTimer: Timer?
    @ObservationIgnored private var testCount = 0
    @ObservationIgnored private var foreground = false

    init() {
        location.onFixes = { [weak self] fixes in self?.received(fixes) }
        location.onIssue = { [weak self] text in self?.locationIssue = text }
        location.onPermissionRevoked = { [weak self] in
            self?.stop()
            self?.refreshLocationPermission()
        }
        location.onAuthorizationChange = { [weak self] in self?.refreshLocationPermission() }
        refreshLocationPermission()
        mesh.onChange = { [weak self] in
            guard let self else { return }
            if self.mesh.fatal { if self.sharing { self.stop() } } else { self.flush() }
        }
    }
    func requestLocationPermission() {
        location.requestPermission()
    }
    func refreshLocationPermission() {
        locationAuthorization = location.authorizationStatus
        locationAccuracy = location.accuracyAuthorization
        if locationAuthorization == .denied || locationAuthorization == .restricted {
            latest = nil
            if sharing && !testMode { stop(); issue = "위치 권한 철회 · Mesh 공유 중지" }
        }
        updateLocationDemand()
    }
    func appBecameActive() {
        foreground = true
        refreshLocationPermission()
    }
    func appEnteredBackground() {
        foreground = false
        if sharing && testMode { stop(); issue = "TEST는 전면 시험 전용입니다. 잠금 시험은 LIVE로 실행하세요." }
        updateLocationDemand()
    }
    private func updateLocationDemand() {
        receivingLocation = location.updateDemand(viewing: foreground, sharing: sharing && !testMode)
    }
    func start() {
        guard !sharing, !mesh.fatal, mesh.selected != nil,
              UIApplication.shared.applicationState == .active else { return }
        if !testMode && locationAuthorization != .authorizedWhenInUse && locationAuthorization != .authorizedAlways {
            requestLocationPermission()
            issue = "위치 권한을 허용한 뒤 Mesh 공유를 시작하세요."
            return
        }
        let id = UInt32.random(in: 1...UInt32.max)
        session.start(sessionID: id); sessionID = id
        sharing = true; issue = nil; submitted = 0; completed = 0
        lastSequence = nil; lastTransmission = nil
        mesh.connect()
        if testMode {
            testCount = 0
            testTimer = Timer.scheduledTimer(withTimeInterval: 1.1, repeats: true) { [weak self] _ in
                Task { @MainActor in self?.testTick() }
            }
        }
        updateLocationDemand()
    }
    func stop() {
        sharing = false; session.stop()
        testTimer?.invalidate(); testTimer = nil
        mesh.disconnect()
        updateLocationDemand()
    }
    private func testTick() {
        guard sharing, mesh.ready, session.inFlight == nil, testCount < 30 else { return }
        testCount += 1
        offerForSharing([LocationFix(latitude: 37.5665, longitude: 126.978, accuracy: 5,
            measuredAt: Date().timeIntervalSince1970, isTest: true)])
        if testCount == 30 { testTimer?.invalidate(); testTimer = nil }
    }
    private func received(_ fixes: [LocationFix]) {
        guard receivingLocation else { return }
        let now = Date().timeIntervalSince1970
        // Show coarse but valid fixes too. Mesh keeps its stricter <=50m policy.
        if let newest = fixes.filter({
            $0.latitude.isFinite && (-90...90).contains($0.latitude) &&
            $0.longitude.isFinite && (-180...180).contains($0.longitude) &&
            $0.accuracy.isFinite && $0.accuracy >= 0 &&
            $0.measuredAt.isFinite && (0...5).contains(now - $0.measuredAt)
        }).max(by: { $0.measuredAt < $1.measuredAt }),
           latest == nil || newest.measuredAt > latest!.measuredAt {
            latest = newest
            locationIssue = nil
        }
        if sharing && !testMode { offerForSharing(fixes) }
    }
    private func offerForSharing(_ fixes: [LocationFix]) {
        guard sharing else { return }
        session.offer(fixes, now: Date().timeIntervalSince1970)
        issue = session.lastIssue
        flush()
    }
    private func flush() {
        guard sharing else { return }
        guard let packet = session.take(now: Date().timeIntervalSince1970,
            monotonic: ProcessInfo.processInfo.systemUptime, connected: mesh.ready) else {
            if let reason = session.lastIssue { issue = reason }; return
        }
        submitted += 1; lastSequence = packet.sequence; sessionID = packet.sessionID
        mesh.send(packet) { [weak self] result in
            guard let self, self.session.inFlight == packet else { return }
            self.session.complete(packet)
            switch result {
            case .success:
                self.completed += 1; self.lastTransmission = Date()
            case .failure:
                self.issue = "SDK 송신 실패 · 이 샘플은 재전송하지 않습니다"
            }
            self.flush()
        }
    }
    func importFile(_ url: URL) {
        guard !sharing else { return }
        let access = url.startAccessingSecurityScopedResource()
        defer { if access { url.stopAccessingSecurityScopedResource() } }
        do {
            let size = try url.resourceValues(forKeys: [.fileSizeKey]).fileSize ?? 0
            guard size <= 2_000_000 else { throw DemoError.message("파일이 너무 큽니다 (최대 2MB).") }
            mesh.disconnect()
            try mesh.importNetwork(Data(contentsOf: url))
            issue = nil
        } catch {
            // Decoder errors can contain key-bearing JSON. Show only our sanitized messages.
            issue = (error as? DemoError)?.errorDescription ?? "네트워크 파일 가져오기 실패. JSON 형식과 모델 설정을 확인하세요."
        }
    }
}
