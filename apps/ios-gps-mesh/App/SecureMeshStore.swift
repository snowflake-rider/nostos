import Foundation
import Security
import NordicMesh
import GPSCore

enum DemoError: LocalizedError {
    case message(String)
    var errorDescription: String? { if case .message(let text) = self { return text }; return nil }
}

struct MeshRecord: Codable {
    var schema = 1
    var database: Data
    var networkID: UUID
    var provisionerID: UUID
    var source: UInt16
    var appKeyIndex: UInt16
    var highWater: UInt32 = 1
}

/// Keys AND SDK database stay in one device-only Keychain item. No JSON secret file.
final class SecureMeshStore: Storage {
    private let lock = NSRecursiveLock()
    private var record: MeshRecord?
    private var failure = false
    var failed: Bool { lock.lock(); defer { lock.unlock() }; return failure }
    private let query: [String: Any] = [
        kSecClass as String: kSecClassGenericPassword,
        kSecAttrService as String: "GPSMesh.private-network.v1",
        kSecAttrAccount as String: "mesh",
        kSecAttrSynchronizable as String: false
    ]

    init() throws {
        var request = query
        request[kSecReturnData as String] = true
        request[kSecMatchLimit as String] = kSecMatchLimitOne
        var result: CFTypeRef?
        let status = SecItemCopyMatching(request as CFDictionary, &result)
        if status == errSecItemNotFound { return }
        guard status == errSecSuccess, let data = result as? Data else {
            throw DemoError.message("보안 저장소 접근 실패 (\(status)). 잠금 해제 후 다시 실행하세요.")
        }
        do {
            record = try JSONDecoder().decode(MeshRecord.self, from: data)
            guard let record, record.schema == 1, (1...0x7fff).contains(record.source),
                  record.appKeyIndex <= 0xfff, !record.database.isEmpty else {
                throw DemoError.message("저장된 Mesh 상태가 유효하지 않습니다. 자동 초기화하지 않습니다.")
            }
        } catch { throw DemoError.message("Mesh 저장 상태를 해석할 수 없습니다. 자동 초기화하지 않습니다.") }
    }
    func snapshot() -> MeshRecord? { lock.lock(); defer { lock.unlock() }; return record }
    func load() -> Data? { snapshot()?.database }
    func save(_ data: Data) -> Bool {
        lock.lock(); defer { lock.unlock() }
        guard var next = record, !failed else { return false }
        next.database = data
        do { try commit(next); return true } catch { failure = true; return false }
    }
    func install(_ next: MeshRecord) throws {
        lock.lock(); defer { lock.unlock() }
        guard record == nil, !failed else { throw DemoError.message("재가져오기는 SEQ 보호를 위해 막혀 있습니다.") }
        try commit(next)
    }
    func reserve(_ high: UInt32) throws {
        lock.lock(); defer { lock.unlock() }
        guard var next = record, !failed, high >= next.highWater else {
            throw DemoError.message("Mesh SEQ 저장 상태가 안전하지 않아 송신을 중단했습니다.")
        }
        next.highWater = high
        do { try commit(next) } catch { failure = true; throw error }
    }
    private func commit(_ next: MeshRecord) throws {
        let data = try JSONEncoder().encode(next)
        var attributes: [String: Any] = [
            kSecValueData as String: data,
            kSecAttrAccessible as String: kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly
        ]
        let status: OSStatus
        if record == nil {
            attributes.merge(query) { value, _ in value }
            status = SecItemAdd(attributes as CFDictionary, nil)
        } else {
            status = SecItemUpdate(query as CFDictionary, attributes as CFDictionary)
        }
        guard status == errSecSuccess else { throw DemoError.message("보안 저장 실패 (\(status)). 송신하지 않습니다.") }
        record = next
    }
}

/// Covers ALL outgoing SDK PDUs (including proxy config/segment ACK), not only GPS.
final class FencedTransmitter: Transmitter {
    private let lock = NSLock()
    private var fence: SequenceFence
    private let manager: MeshNetworkManager
    private let store: SecureMeshStore
    private let bearer: GattBearer
    private var failed = false
    var onFatalError: (() -> Void)?

    init(manager: MeshNetworkManager, store: SecureMeshStore, bearer: GattBearer,
         fence: SequenceFence) {
        self.manager = manager; self.store = store; self.bearer = bearer; self.fence = fence
    }
    func send(_ data: Data, ofType type: PduType) throws {
        lock.lock(); defer { lock.unlock() }
        guard !failed, !store.failed, bearer.isOpen else { throw BearerError.bearerClosed }
        do {
            guard let element = manager.meshNetwork?.localProvisioner?.node?.primaryElement,
                  let sequence = manager.getSequenceNumber(ofLocalElement: element) else {
                throw SequenceFenceError.rollbackOrExhausted
            }
            try fence.beforeTransmission(nextSequence: sequence) { try store.reserve($0) }
        } catch {
            failed = true
            DispatchQueue.main.async { [weak self] in self?.onFatalError?() }
            throw error
        }
        try bearer.send(data, ofType: type)
    }
}
