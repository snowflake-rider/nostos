import Foundation

/// All mutations happen on the owning app's serial executor. No timer or radio dependency.
public struct ShareSession: Sendable {
    public private(set) var active = false
    public private(set) var sessionID: UInt32 = 0
    public private(set) var lastIssue: String?
    public private(set) var inFlight: GPSPacket?
    private var pending: LocationFix?
    private var lastMeasurement: Double = -.infinity
    private var lastSubmission: Double = -.infinity
    private var nextSequence: UInt32 = 1

    public init() {}
    public mutating func start(sessionID: UInt32) {
        self = ShareSession()
        self.sessionID = sessionID; active = sessionID != 0
    }
    public mutating func stop() {
        active = false; pending = nil; inFlight = nil
    }
    public mutating func offer(_ fixes: [LocationFix], now: Double) {
        guard active else { return }
        guard let newest = fixes.filter({ $0.isFresh(at: now) && $0.measuredAt > lastMeasurement })
            .max(by: { $0.measuredAt < $1.measuredAt }) else {
            lastIssue = "유효한 새 위치 대기 (정확도 ≤50m, 나이 ≤5초)"; return
        }
        pending = newest; lastMeasurement = newest.measuredAt; lastIssue = nil
    }
    public mutating func take(now: Double, monotonic: Double, connected: Bool,
                              newSessionID: () -> UInt32 = { UInt32.random(in: 1...UInt32.max) }) -> GPSPacket? {
        guard active, inFlight == nil, let fix = pending else { return nil }
        guard fix.isFresh(at: now) else {
            pending = nil; lastIssue = "오래되었거나 미래 시각인 위치 폐기"; return nil
        }
        guard connected, monotonic.isFinite, monotonic - lastSubmission >= 1 else { return nil }
        if nextSequence == 0 {
            let candidate = newSessionID()
            guard candidate != 0, candidate != sessionID else {
                lastIssue = "새 세션 ID 생성 실패"; return nil
            }
            sessionID = candidate; nextSequence = 1
        }
        guard let packet = try? GPSPacket(fix: fix, sessionID: sessionID, sequence: nextSequence) else { return nil }
        nextSequence &+= 1
        lastSubmission = monotonic; pending = nil; inFlight = packet
        return packet
    }
    public mutating func complete(_ packet: GPSPacket) {
        if inFlight == packet { inFlight = nil }
    }
}

public struct ReconnectBackoff: Sendable {
    private var failures = 0
    public init() {}
    public mutating func nextDelay() -> Double {
        defer { failures = min(failures + 1, 5) }
        return min(pow(2, Double(failures)), 30)
    }
    public mutating func reset() { failures = 0 }
}
