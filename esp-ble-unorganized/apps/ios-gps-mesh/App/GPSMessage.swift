import Foundation
import NordicMesh
import GPSCore

struct GPSMessage: StaticMeshMessage, UnacknowledgedMeshMessage {
    // Nordic's UInt32 opcode is byte order F0 E5 02 (not F0 02 E5).
    static let opCode: UInt32 = 0xF0E502
    let packet: GPSPacket
    var parameters: Data? { packet.encoded() }
    init(packet: GPSPacket) { self.packet = packet }
    init?(parameters: Data) {
        guard let packet = try? GPSPacket.decode(parameters) else { return nil }
        self.packet = packet
    }
}

final class GPSClientDelegate: ModelDelegate {
    let messageTypes: [UInt32: MeshMessage.Type] = [:] // TX only; no pretend GPS ACK.
    let isSubscriptionSupported = false
    let publicationMessageComposer: MessageComposer? = nil
    func model(_ model: Model, didReceiveAcknowledgedMessage request: any AcknowledgedMeshMessage,
               from source: Address, sentTo destination: MeshAddress) throws -> any MeshResponse { throw ModelError.invalidMessage }
    func model(_ model: Model, didReceiveUnacknowledgedMessage message: any UnacknowledgedMeshMessage,
               from source: Address, sentTo destination: MeshAddress) {}
    func model(_ model: Model, didReceiveResponse response: any MeshResponse,
               toAcknowledgedMessage request: any AcknowledgedMeshMessage, from source: Address) {}
}
