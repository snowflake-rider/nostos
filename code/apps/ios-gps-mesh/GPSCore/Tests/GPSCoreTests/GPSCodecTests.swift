import Foundation
import Testing
import GPSCore

@Test func goldenVector() throws {
    let fix = LocationFix(latitude: 37.5665, longitude: 126.978, accuracy: 5,
                          measuredAt: 1_700_000_000, isTest: true)
    let packet = try GPSPacket(fix: fix, sessionID: 0x01020304, sequence: 1)
    let golden = Data([0x01,0x01,0x32,0x00,0x04,0x03,0x02,0x01,
                       0x01,0x00,0x00,0x00,0x00,0xf1,0x53,0x65,
                       0x68,0x31,0x64,0x16,0x20,0x4e,0xaf,0x4b])
    #expect(packet.encoded() == golden)
    #expect(try GPSPacket.decode(golden) == packet)
}

@Test func boundariesAndInvalidFields() throws {
    for (lat, lon) in [(0.0, 0.0), (-90.0, -180.0), (90.0, 180.0)] {
        let p = try GPSPacket(fix: .init(latitude: lat, longitude: lon, accuracy: 50,
            measuredAt: Double(UInt32.max) + 0.5), sessionID: .max, sequence: .max)
        #expect(try GPSPacket.decode(p.encoded()) == p)
    }
    for lat in [Double.nan, .infinity, -90.1, 90.1] {
        #expect(throws: GPSCodecError.self) {
            try GPSPacket(fix: .init(latitude: lat, longitude: 0, accuracy: 0, measuredAt: 1), sessionID: 1, sequence: 1)
        }
    }
    for accuracy in [Double.nan, .infinity, -1, 50.01] {
        #expect(!LocationFix(latitude: 0, longitude: 0, accuracy: accuracy, measuredAt: 1).isValid)
    }
    for timestamp in [Double.nan, .infinity, 0, -1, Double(UInt32.max) + 1] {
        #expect(!LocationFix(latitude: 0, longitude: 0, accuracy: 0, measuredAt: timestamp).isValid)
    }
    let p = try GPSPacket(fix: .init(latitude: -0.00000005, longitude: 0.00000005,
        accuracy: 0.01, measuredAt: 1.9), sessionID: 1, sequence: 1)
    #expect(p.latitudeE7 == -1 && p.longitudeE7 == 1 && p.accuracyDM == 1 && p.measuredAt == 1)
    for count in [0, 23, 25] { #expect(throws: GPSCodecError.self) { try GPSPacket.decode(Data(repeating: 0, count: count)) } }
    for (offset, value) in [(0, UInt8(2)), (1, 2), (3, 3), (4, 0), (8, 0), (12, 0)] {
        var bad = p.encoded(); bad[offset] = value
        #expect(throws: GPSCodecError.self) { try GPSPacket.decode(bad) }
    }
    var padded = Data([99]); padded.append(p.encoded())
    #expect(try GPSPacket.decode(padded.dropFirst()) == p)
}
