import Testing
import GPSCore

@Test func persistsBeforeTransmissionAndSurvivesRestart() throws {
    var persisted: UInt32 = 100
    var fence = try SequenceFence(reservedThrough: persisted)
    try fence.beforeTransmission(nextSequence: 101) { persisted = $0 }
    #expect(persisted > 101)
    let restarted = try SequenceFence(reservedThrough: persisted)
    #expect(restarted.restartSequence == persisted)
    var afterCrash = restarted
    #expect(throws: SequenceFenceError.self) { try afterCrash.beforeTransmission(nextSequence: 101) { _ in } }
}

@Test func persistenceFailureCannotAdvanceReservation() throws {
    enum Failure: Error { case disk }
    var fence = try SequenceFence(reservedThrough: 1)
    #expect(throws: Failure.self) { try fence.beforeTransmission(nextSequence: 2) { _ in throw Failure.disk } }
    var saved = false
    try fence.beforeTransmission(nextSequence: 2) { _ in saved = true }
    #expect(saved)
    #expect(throws: SequenceFenceError.self) { try fence.beforeTransmission(nextSequence: 0x1000000) { _ in } }
}
