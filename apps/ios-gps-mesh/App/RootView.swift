import SwiftUI
import UniformTypeIdentifiers
import CoreLocation
import UIKit

struct RootView: View {
    @Bindable var model: AppModel
    @Environment(\.scenePhase) private var scenePhase
    var body: some View {
        NavigationStack {
            Form {
                Section {
                    LocationMapView(fix: model.latest)
                        .listRowInsets(EdgeInsets())
                } header: { Text("현재 위치 지도") } footer: {
                    Text("지도 배경은 인터넷을 사용합니다. GPS 값 표시와 Bluetooth Mesh 전송은 별개입니다.")
                }
                Section("iPhone GPS") {
                    LocationReadout(fix: model.latest, receiving: model.receivingLocation)
                    if let issue = model.locationIssue {
                        Text(issue).font(.footnote).foregroundStyle(.orange)
                    }
                }
                Section {
                    Label(model.sharing ? (model.testMode ? "TEST 공유 중" : "Mesh 공유 중") : "Mesh 공유 중지됨",
                          systemImage: model.sharing ? "location.fill" : "location.slash")
                        .font(.title2.weight(.semibold))
                    Text(model.mesh.status).foregroundStyle(.secondary)
                    Button(model.sharing ? "Mesh 공유 중지" : "Mesh 공유 시작") {
                        if model.sharing { model.stop() } else { model.start() }
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(model.sharing ? .orange : .blue)
                    .disabled(!model.sharing && (model.mesh.selected == nil || model.mesh.fatal))
                    .accessibilityIdentifier("mesh.sharing.toggle")
                    if let issue = model.issue { Text(issue).font(.footnote).foregroundStyle(.orange) }
                } footer: {
                    Text("LIVE는 화면 잠금 중 위치 공유를 요청합니다. 강제 종료 후 자동 재개하지 않습니다.")
                }
                LocationPermissionSection(model: model)
                Section("Bluetooth Mesh") {
                    LabeledContent("네트워크", value: model.mesh.networkName)
                    LabeledContent("내 송신 주소", value: hex(model.mesh.source))
                    LabeledContent("그룹", value: "0xC000")
                    LabeledContent("AppKey index", value: hex(model.mesh.appKeyIndex))
                    LabeledContent("선택 Proxy", value: model.mesh.selected?.name ?? "미선택")
                    LabeledContent("확인된 Proxy 주소", value: hex(model.mesh.proxyAddress))
                }
                Section {
                    LabeledContent("SDK 제출", value: "\(model.submitted)")
                    LabeledContent("SDK 송신 완료", value: "\(model.completed)")
                    LabeledContent("Session", value: model.sessionID.map { String(format: "%08X", $0) } ?? "—")
                    LabeledContent("마지막 sample_seq", value: model.lastSequence.map(String.init) ?? "—")
                    if let time = model.lastTransmission { LabeledContent("마지막 SDK 완료") { Text(time, style: .time) } }
                } header: { Text("송신 진단") } footer: {
                    Text("SDK 완료는 ESP32 수신 확인이 아닙니다. 각 보드의 GPS_RX 로그를 확인하세요. 위치 이력은 저장하지 않습니다.")
                }
            }
            .navigationTitle("GPS Mesh")
            .toolbar {
                NavigationLink { SettingsView(model: model) } label: { Label("설정", systemImage: "gearshape") }
                    .disabled(model.sharing)
            }
        }
        .onAppear {
            if scenePhase == .active { model.appBecameActive() }
        }
        .onChange(of: scenePhase) { _, phase in
            if phase == .active { model.appBecameActive() }
            if phase == .background { model.appEnteredBackground() }
        }
    }
}

private struct LocationPermissionSection: View {
    let model: AppModel
    @Environment(\.openURL) private var openURL

    private var status: String {
        switch model.locationAuthorization {
        case .notDetermined: "미요청"
        case .authorizedWhenInUse: "앱 사용 중 허용"
        case .authorizedAlways: "항상 허용"
        case .denied: "거부됨"
        case .restricted: "제한됨"
        @unknown default: "확인 필요"
        }
    }

    var body: some View {
        Section {
            LabeledContent("허용 상태", value: status)
            if model.locationAuthorization == .authorizedWhenInUse || model.locationAuthorization == .authorizedAlways {
                LabeledContent("정밀 위치", value: model.locationAccuracy == .fullAccuracy ? "켬" : "끔")
            }
            if model.locationAuthorization == .notDetermined {
                Button("위치 권한 허용") { model.requestLocationPermission() }
                    .accessibilityIdentifier("location.permission.request")
            } else if model.locationAuthorization == .restricted {
                Text("기기 제한으로 위치를 사용할 수 없습니다. 스크린 타임 또는 기기 관리 설정을 확인하세요.")
                    .font(.footnote)
            } else {
                Button("앱 설정 열기") {
                    if let url = URL(string: UIApplication.openSettingsURLString) { openURL(url) }
                }
            }
        } header: { Text("위치 권한") } footer: {
            Text("권한을 허용하면 앱을 보는 동안 GPS와 지도를 갱신합니다. Mesh 공유는 별도로 시작해야 합니다.")
        }
    }
}

private struct SettingsView: View {
    @Bindable var model: AppModel
    @State private var importing = false
    var body: some View {
        Form {
            Section {
                Button("nRF Mesh JSON 가져오기") { importing = true }
                    .disabled(model.mesh.source != nil || model.mesh.fatal)
                Text("먼저 세 ESP32의 GPS Vendor Model을 AppKey에 bind하고 0xC000에 subscribe하세요. 기존 키 원문은 표시하지 않습니다.")
                    .font(.footnote).foregroundStyle(.secondary)
                if let source = model.mesh.source {
                    Text("각 보드 시리얼에 입력")
                    Text(String(format: "gps-source 0x%04X", source)).font(.system(.body, design: .monospaced)).textSelection(.enabled)
                    Text("이 주소는 이 앱 전용입니다. 다른 provisioner에서 재할당하지 마세요.").font(.footnote)
                }
            } header: { Text("네트워크 설정") } footer: {
                Text("첫 버전은 재가져오기·초기화·키 내보내기를 제공하지 않습니다. 올바른 최신 파일을 사용하세요.")
            }
            Section("지정 Proxy") {
                Button("같은 네트워크의 Proxy 검색") { model.mesh.scan() }.disabled(model.mesh.source == nil || model.mesh.fatal)
                ForEach(model.mesh.candidates) { candidate in
                    Button { model.mesh.select(candidate) } label: {
                        VStack(alignment: .leading, spacing: 4) {
                            Text(candidate.name)
                            Text(candidate.nodeAddress.map { hex($0) } ?? "주소는 연결 후 확인")
                                .font(.caption).foregroundStyle(.secondary)
                            Text(candidate.id.uuidString.suffix(8)).font(.caption.monospaced()).foregroundStyle(.secondary)
                        }
                    }
                }
                Button("선택 Proxy 연결 확인") { model.mesh.connect() }.disabled(model.mesh.selected == nil || model.mesh.fatal)
                Button("연결 해제") { model.mesh.disconnect() }
                Text(model.mesh.status).font(.footnote)
            }
            Section("설정에 포함된 GPS 노드 · 온라인 상태 아님") {
                ForEach(model.mesh.configuredNodes, id: \.self) { node in Text(hex(node)).monospaced() }
            }
            Section {
                Toggle("TEST 고정 좌표 사용", isOn: $model.testMode)
            } header: { Text("개발 시험") } footer: {
                Text("TEST는 서울시청 좌표 30개를 현재 시각으로 생성하는 전면 시험입니다. 화면 잠금 시험은 TEST를 끄고 LIVE로 실행하세요.")
            }
            if let issue = model.issue { Text(issue).foregroundStyle(.orange) }
        }
        .navigationTitle("설정").navigationBarTitleDisplayMode(.inline)
        .disabled(model.sharing)
        .fileImporter(isPresented: $importing, allowedContentTypes: [.json]) { result in
            switch result {
            case .success(let url): model.importFile(url)
            case .failure: model.issue = "파일 선택을 완료하지 못했습니다."
            }
        }
    }
}

private func hex(_ address: UInt16?) -> String { address.map { String(format: "0x%04X", $0) } ?? "—" }
