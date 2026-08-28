import Foundation
import CoreBluetooth
import Observation
import NordicMesh
import GPSCore

struct ProxyCandidate: Identifiable {
    let id: UUID
    let name: String
    let nodeAddress: UInt16?
}

/// UI/connection state is confined to the main queue; SDK outbound fence owns its lock.
@Observable final class MeshTransport: NSObject, CBCentralManagerDelegate, BearerDelegate, ProxyFilterDelegate {
    private(set) var networkName = "네트워크 없음"
    private(set) var source: UInt16?
    private(set) var appKeyIndex: UInt16?
    private(set) var configuredNodes: [UInt16] = []
    private(set) var candidates: [ProxyCandidate] = []
    private(set) var selected: ProxyCandidate?
    private(set) var proxyAddress: UInt16?
    private(set) var ready = false
    private(set) var status = "설정 파일을 가져오세요"
    private(set) var fatal = false
    @ObservationIgnored var onChange: (() -> Void)?
    @ObservationIgnored private var central: CBCentralManager!
    @ObservationIgnored private var manager: MeshNetworkManager!
    @ObservationIgnored private var store: SecureMeshStore?
    @ObservationIgnored private let clientDelegate = GPSClientDelegate()
    @ObservationIgnored private var bearer: GattBearer?
    @ObservationIgnored private var fenced: FencedTransmitter?
    @ObservationIgnored private var backoff = ReconnectBackoff()
    @ObservationIgnored private var reconnectWork: DispatchWorkItem?
    @ObservationIgnored private var connectionTimeout: DispatchWorkItem?
    @ObservationIgnored private var wantsConnection = false
    @ObservationIgnored private var scanningForSelection = false
    @ObservationIgnored private var expectedProxy: UInt16?

    override init() {
        super.init()
        do {
            let secure = try SecureMeshStore(); store = secure
            manager = MeshNetworkManager(using: secure)
            if let record = secure.snapshot() {
                guard try manager.load(), let network = manager.meshNetwork,
                      network.uuid == record.networkID,
                      let provisioner = network.provisioners.first(where: { $0.uuid == record.provisionerID }),
                      provisioner.primaryUnicastAddress == record.source else {
                    throw DemoError.message("저장된 로컬 송신자 정보 불일치. 초기화하지 않습니다.")
                }
                try network.setLocalProvisioner(provisioner)
                configureLocalModels()
                try validateDestinations(network, requiredKey: record.appKeyIndex)
                _ = try SequenceFence(reservedThrough: record.highWater)
                updateSummary(network)
                status = "네트워크 복원됨 · Proxy를 선택하세요"
            }
        } catch { fatal = true; status = error.localizedDescription }
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func importNetwork(_ data: Data) throws {
        guard !fatal, !wantsConnection, let store, store.snapshot() == nil else {
            throw DemoError.message("기존 네트워크 재가져오기는 주소/SEQ 보호를 위해 지원하지 않습니다.")
        }
        guard data.count <= 2_000_000,
              let json = try JSONSerialization.jsonObject(with: data) as? [String: Any],
              json["nodes"] is [[String: Any]], json["provisioners"] is [[String: Any]] else {
            throw DemoError.message("유효한 nRF Mesh JSON 파일이 아닙니다.")
        }
        // No transmitter is attached while importing or allocating an identity.
        manager = MeshNetworkManager(using: store)
        let network = try manager.import(from: data)
        try validateDestinations(network, requiredKey: nil)
        guard let address = (1...0x7fff).reversed().map({ UInt16($0) }).first(where: { address in
            !network.provisioners.contains { $0.allocatedUnicastRange.contains { $0.range.contains(address) } } &&
            !network.nodes.contains { $0.contains(elementWithAddress: address) }
        }) else { throw DemoError.message("겹치지 않는 새 송신자 주소가 없습니다. 기존 주소를 복제하지 않습니다.") }
        let provisioner = Provisioner(name: "GPS Mesh iPhone", allocatedUnicastRange: [AddressRange(from: address, to: address)],
            allocatedGroupRange: [], allocatedSceneRange: [])
        try network.setLocalProvisioner(provisioner)
        configureLocalModels()
        guard let key = appKeyIndex else { throw DemoError.message("공통 AppKey를 확인하세요.") }
        // Capture SDK-private database via Storage without putting key JSON on disk.
        let capture = CapturingStorage()
        let encoderManager = MeshNetworkManager(using: capture)
        let copied = try encoderManager.import(from: manager.export())
        guard let local = copied.provisioners.first(where: { $0.uuid == provisioner.uuid }) else { throw DemoError.message("송신자 저장 실패") }
        try copied.setLocalProvisioner(local)
        guard encoderManager.save(), let database = capture.data else { throw DemoError.message("네트워크 저장 실패") }
        try store.install(MeshRecord(database: database, networkID: network.uuid,
            provisionerID: provisioner.uuid, source: address, appKeyIndex: key))
        guard manager.save() else { failClosed("네트워크 저장 실패"); return }
        updateSummary(network)
        status = "가져오기 완료 · 각 보드에 gps-source 주소를 설정하세요"
    }

    private func configureLocalModels() {
        manager.localElements = [Element(name: "GPS", models: [Model(vendorModelId: 0x1000, companyId: 0x02E5, delegate: clientDelegate)])]
        manager.proxyFilter.delegate = self
    }
    private func validateDestinations(_ network: MeshNetwork, requiredKey: UInt16?) throws {
        guard let group = network.groups.first(where: { $0.address.address == 0xc000 }) else {
            throw DemoError.message("설정에 그룹 0xC000이 없습니다.")
        }
        var servers: [(UInt16, Model)] = []
        for node in network.nodes where !node.isLocalProvisioner {
            for element in node.elements {
                for model in element.models where model.companyIdentifier == 0x02e5 && model.modelIdentifier == 0x1001 {
                    if model.subscriptions.contains(group) { servers.append((node.primaryUnicastAddress, model)) }
                }
            }
        }
        guard servers.count == 3, Set(servers.map { $0.0 }).count == 3 else {
            throw DemoError.message("세 보드의 GPS 모델(02E5:1001)·0xC000 구독을 설정한 뒤 새 JSON을 가져오세요.")
        }
        let common = network.applicationKeys.filter { key in servers.allSatisfy { key.isBound(to: $0.1) } }
        guard let key = common.first(where: { requiredKey == nil || $0.index == requiredKey }),
              requiredKey != nil || common.count == 1 else {
            throw DemoError.message("세 GPS 모델에 공통으로 연결된 AppKey 하나를 확인할 수 없습니다.")
        }
        appKeyIndex = key.index; configuredNodes = servers.map { $0.0 }.sorted()
    }
    private func updateSummary(_ network: MeshNetwork) {
        networkName = network.meshName
        source = network.localProvisioner?.primaryUnicastAddress
    }

    func scan() {
        guard !fatal, store?.snapshot() != nil, central.state == .poweredOn, !wantsConnection else { return }
        candidates = []; scanningForSelection = true
        central.scanForPeripherals(withServices: [MeshProxyService.uuid], options: nil)
        status = "이 네트워크의 Proxy 검색 중"
    }
    func select(_ candidate: ProxyCandidate) {
        guard !fatal else { return }
        disconnect()
        selected = candidate; expectedProxy = candidate.nodeAddress
        scanningForSelection = false; central.stopScan()
        status = "Proxy 선택됨 · 연결하세요"
    }
    func connect() {
        guard !fatal, selected != nil, !wantsConnection else { return }
        wantsConnection = true; backoff.reset(); beginConnection()
    }
    func disconnect() {
        wantsConnection = false; reconnectWork?.cancel(); connectionTimeout?.cancel()
        central?.stopScan(); scanningForSelection = false; ready = false
        manager?.transmitter = nil; manager?.proxyFilter.proxyDidDisconnect()
        bearer?.delegate = nil; bearer?.close(); bearer = nil; fenced = nil; proxyAddress = nil
        if !fatal { status = "Proxy 연결 해제됨" }
        onChange?()
    }
    private func beginConnection() {
        guard wantsConnection, !fatal, let selected, let store, let record = store.snapshot(),
              central.state == .poweredOn else { return }
        do {
            let fence = try SequenceFence(reservedThrough: record.highWater)
            guard let element = manager.meshNetwork?.localProvisioner?.node?.primaryElement else { throw DemoError.message("로컬 Element 없음") }
            manager.setSequenceNumber(fence.restartSequence, forLocalElement: element)
            let link = GattBearer(targetWithIdentifier: selected.id)
            let gate = FencedTransmitter(manager: manager, store: store, bearer: link, fence: fence)
            gate.onFatalError = { [weak self] in self?.failClosed("SEQ 보호/보안 저장 오류. 앱 재실행 후 상태를 확인하세요.") }
            link.delegate = self; link.dataDelegate = manager
            // Intentionally no SDK logger: it includes key material and payloads.
            bearer = link; fenced = gate; manager.transmitter = gate
            status = "지정 Proxy 연결 중"; link.open()
            let timeout = DispatchWorkItem { [weak self, weak link] in
                guard let self, let link, self.bearer === link, !self.ready else { return }
                self.connectionFailed()
            }
            connectionTimeout = timeout; DispatchQueue.main.asyncAfter(deadline: .now() + 25, execute: timeout)
        } catch { failClosed(error.localizedDescription) }
    }
    private func connectionFailed() {
        ready = false; proxyAddress = nil
        connectionTimeout?.cancel(); manager.transmitter = nil; manager.proxyFilter.proxyDidDisconnect()
        bearer?.delegate = nil; bearer?.close(); bearer = nil; fenced = nil
        guard wantsConnection, !fatal else { onChange?(); return }
        let delay = backoff.nextDelay()
        status = "연결 대기 · 재시도 \(Int(delay))초 이후"
        let work = DispatchWorkItem { [weak self] in self?.beginConnection() }
        reconnectWork?.cancel(); reconnectWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + delay, execute: work)
        onChange?()
    }
    private func failClosed(_ text: String) {
        fatal = true; disconnect(); status = text; onChange?()
    }
    func send(_ packet: GPSPacket, completion: @escaping (Result<Void, Error>) -> Void) {
        guard ready, !fatal, let index = appKeyIndex,
              let key = manager.meshNetwork?.applicationKeys.first(where: { $0.index == index }) else {
            completion(.failure(BearerError.bearerClosed)); return
        }
        do {
            try manager.send(GPSMessage(packet: packet), to: MeshAddress(0xc000), withTtl: 7, using: key, completion: completion)
        } catch { completion(.failure(error)) }
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        guard !fatal else { return }
        if central.state != .poweredOn {
            if wantsConnection { connectionFailed() }
            ready = false; status = "Bluetooth를 켜고 권한을 확인하세요"; onChange?()
        } else if wantsConnection { reconnectWork?.cancel(); beginConnection() }
    }
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        guard scanningForSelection, let network = manager?.meshNetwork else { return }
        let node = advertisementData.nodeIdentity.flatMap { network.node(matchingNodeIdentity: $0) }
        let matches = node != nil || advertisementData.networkIdentity.map { network.matches(networkIdentity: $0) } == true
        guard matches, !candidates.contains(where: { $0.id == peripheral.identifier }) else { return }
        candidates.append(.init(id: peripheral.identifier,
            name: peripheral.name ?? "Mesh Proxy", nodeAddress: node?.primaryUnicastAddress))
    }
    func bearerDidOpen(_ bearer: Bearer) {
        guard let current = self.bearer, bearer === current else { return }
        status = "GATT 연결됨 · Mesh 인증/Proxy 주소 확인 중"
        // SDK authenticates beacons and configures proxy filter from incoming data.
    }
    func bearer(_ bearer: Bearer, didClose error: Error?) {
        guard let current = self.bearer, bearer === current else { return }
        connectionFailed()
    }
    func proxyFilterUpdated(type: ProxyFilerType, addresses: Set<Address>) {}
    func proxyFilterUpdateAcknowledged(type: ProxyFilerType, listSize: UInt16) {
        guard wantsConnection, bearer?.isOpen == true, !fatal, manager.proxyFilter.proxy != nil else { return }
        guard let node = manager.proxyFilter.proxy, configuredNodes.contains(node.primaryUnicastAddress),
              expectedProxy == nil || expectedProxy == node.primaryUnicastAddress else {
            failClosed("선택 Proxy와 가져온 노드 정보가 일치하지 않습니다."); return
        }
        expectedProxy = node.primaryUnicastAddress; proxyAddress = node.primaryUnicastAddress
        ready = true; backoff.reset(); connectionTimeout?.cancel()
        status = "Mesh Proxy 준비됨"; onChange?()
    }
}

private final class CapturingStorage: Storage {
    var data: Data?
    func load() -> Data? { data }
    func save(_ data: Data) -> Bool { self.data = data; return true }
}
