import Testing
import GPSCore

private func fix(_ time: Double, lat: Double = 1) -> LocationFix {
    .init(latitude: lat, longitude: 2, accuracy: 5, measuredAt: time)
}

@Test func latestOnlyThrottleAndStop() throws {
    var session = ShareSession()
    session.start(sessionID: 7)
    session.offer([fix(100)], now: 100)
    let firstResult = session.take(now: 100, monotonic: 10, connected: true)
    let first = try #require(firstResult)
    session.offer([fix(100.2, lat: 3), fix(100.1, lat: 2)], now: 100.2)
    #expect(session.take(now: 100.2, monotonic: 10.2, connected: true) == nil)
    session.complete(first)
    #expect(session.take(now: 100.2, monotonic: 10.2, connected: true) == nil)
    let nextResult = session.take(now: 101, monotonic: 11, connected: true)
    let next = try #require(nextResult)
    #expect(next.sequence == 2)
    #expect(next.latitudeE7 == 30_000_000)
    session.offer([fix(101)], now: 101)
    session.stop()
    session.complete(next)
    #expect(session.take(now: 102, monotonic: 12, connected: true) == nil)
}

@Test func reconnectBackoffCapsAndResets() {
    var backoff = ReconnectBackoff()
    let delays = (0..<8).map { _ in backoff.nextDelay() }
    #expect(delays == [1, 2, 4, 8, 16, 30, 30, 30])
    backoff.reset()
    let delay = backoff.nextDelay()
    #expect(delay == 1)
}

@Test func staleDisconnectedAndInvalidFixes() throws {
    var session = ShareSession(); session.start(sessionID: 2)
    session.offer([fix(100)], now: 100)
    #expect(session.take(now: 100, monotonic: 1, connected: false) == nil)
    #expect(session.take(now: 106, monotonic: 7, connected: true) == nil)
    session.offer([fix(107), fix(106, lat: 1000), fix(90)], now: 106)
    #expect(session.take(now: 106, monotonic: 7, connected: true) == nil)
    session.offer([fix(107)], now: 107)
    let packetResult = session.take(now: 112, monotonic: 13, connected: true)
    let packet = try #require(packetResult)
    session.complete(packet) // A failed SDK send also releases the slot; never retries this sample.
    session.offer([fix(107)], now: 112)
    #expect(session.take(now: 112, monotonic: 14, connected: true) == nil)
    session.start(sessionID: 3)
    session.offer([fix(113)], now: 113)
    let currentResult = session.take(now: 113, monotonic: 15, connected: true)
    let current = try #require(currentResult)
    session.complete(packet) // Completion from an old session cannot release current in-flight.
    session.offer([fix(114)], now: 114)
    #expect(session.take(now: 114, monotonic: 16, connected: true) == nil)
    session.complete(current)
    #expect(session.take(now: 114, monotonic: 16, connected: true)?.sequence == 2)
}
