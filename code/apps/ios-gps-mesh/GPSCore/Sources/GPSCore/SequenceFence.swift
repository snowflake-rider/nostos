public enum SequenceFenceError: Error { case rollbackOrExhausted }

/// A crash-safe high-water reservation. The caller MUST persist before emitting bytes.
/// Conservative across IV changes: a sequence reset halts transmission, requiring reload.
public struct SequenceFence: Sendable {
    public let restartSequence: UInt32
    private var reservedThrough: UInt32
    private var lastObserved: UInt32
    public init(reservedThrough: UInt32) throws {
        guard reservedThrough > 0, reservedThrough < 0x00ff_ff00 else { throw SequenceFenceError.rollbackOrExhausted }
        self.restartSequence = reservedThrough
        self.reservedThrough = reservedThrough
        lastObserved = reservedThrough
    }
    public mutating func beforeTransmission(nextSequence: UInt32,
        persist: (UInt32) throws -> Void) throws {
        guard nextSequence > restartSequence, nextSequence >= lastObserved,
              nextSequence < 0x00ff_ff00 else { throw SequenceFenceError.rollbackOrExhausted }
        if nextSequence >= reservedThrough {
            let high = nextSequence + 256
            try persist(high)
            reservedThrough = high
        }
        lastObserved = nextSequence
    }
}
