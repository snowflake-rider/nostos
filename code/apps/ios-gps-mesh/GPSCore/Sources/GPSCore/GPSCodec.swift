import Foundation

public struct LocationFix: Equatable, Sendable {
    public let latitude: Double
    public let longitude: Double
    public let accuracy: Double
    public let measuredAt: TimeInterval
    public let isTest: Bool

    public init(latitude: Double, longitude: Double, accuracy: Double,
                measuredAt: TimeInterval, isTest: Bool = false) {
        self.latitude = latitude; self.longitude = longitude
        self.accuracy = accuracy; self.measuredAt = measuredAt; self.isTest = isTest
    }

    public var isValid: Bool {
        latitude.isFinite && (-90...90).contains(latitude) &&
        longitude.isFinite && (-180...180).contains(longitude) &&
        accuracy.isFinite && (0...50).contains(accuracy) &&
        measuredAt.isFinite && measuredAt >= 1 && measuredAt < Double(UInt32.max) + 1
    }

    public func isFresh(at now: TimeInterval) -> Bool {
        isValid && now.isFinite && (0...5).contains(now - measuredAt)
    }
}

public enum GPSCodecError: Error { case invalidLength, invalidFields }

public struct GPSPacket: Equatable, Sendable {
    public static let byteCount = 24
    public let flags: UInt8
    public let accuracyDM: UInt16
    public let sessionID: UInt32
    public let sequence: UInt32
    public let measuredAt: UInt32
    public let latitudeE7: Int32
    public let longitudeE7: Int32

    public init(fix: LocationFix, sessionID: UInt32, sequence: UInt32) throws {
        guard fix.isValid, sessionID != 0, sequence != 0 else { throw GPSCodecError.invalidFields }
        flags = fix.isTest ? 1 : 0
        accuracyDM = UInt16(ceil(fix.accuracy * 10))
        self.sessionID = sessionID; self.sequence = sequence
        measuredAt = UInt32(floor(fix.measuredAt))
        latitudeE7 = Int32((fix.latitude * 10_000_000).rounded(.toNearestOrAwayFromZero))
        longitudeE7 = Int32((fix.longitude * 10_000_000).rounded(.toNearestOrAwayFromZero))
    }

    private init(flags: UInt8, accuracyDM: UInt16, sessionID: UInt32, sequence: UInt32,
                 measuredAt: UInt32, latitudeE7: Int32, longitudeE7: Int32) {
        self.flags = flags; self.accuracyDM = accuracyDM; self.sessionID = sessionID
        self.sequence = sequence; self.measuredAt = measuredAt
        self.latitudeE7 = latitudeE7; self.longitudeE7 = longitudeE7
    }

    public func encoded() -> Data {
        var bytes = [UInt8](repeating: 0, count: Self.byteCount)
        bytes[0] = 1; bytes[1] = flags
        func put(_ value: UInt32, _ offset: Int, _ count: Int = 4) {
            for i in 0..<count { bytes[offset + i] = UInt8(truncatingIfNeeded: value >> (i * 8)) }
        }
        put(UInt32(accuracyDM), 2, 2); put(sessionID, 4); put(sequence, 8)
        put(measuredAt, 12); put(UInt32(bitPattern: latitudeE7), 16)
        put(UInt32(bitPattern: longitudeE7), 20)
        return Data(bytes)
    }

    public static func decode(_ data: Data) throws -> GPSPacket {
        guard data.count == byteCount else { throw GPSCodecError.invalidLength }
        let bytes = Array(data) // Data slices need not have a zero startIndex.
        func get(_ offset: Int, _ count: Int = 4) -> UInt32 {
            (0..<count).reduce(0) { $0 | (UInt32(bytes[offset + $1]) << ($1 * 8)) }
        }
        let packet = GPSPacket(flags: bytes[1], accuracyDM: UInt16(get(2, 2)),
            sessionID: get(4), sequence: get(8), measuredAt: get(12),
            latitudeE7: Int32(bitPattern: get(16)), longitudeE7: Int32(bitPattern: get(20)))
        guard bytes[0] == 1, packet.flags <= 1, packet.accuracyDM <= 500,
              packet.sessionID != 0, packet.sequence != 0, packet.measuredAt != 0,
              (-900_000_000...900_000_000).contains(packet.latitudeE7),
              (-1_800_000_000...1_800_000_000).contains(packet.longitudeE7)
        else { throw GPSCodecError.invalidFields }
        return packet
    }
}
