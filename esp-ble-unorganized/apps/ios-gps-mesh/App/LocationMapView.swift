import SwiftUI
import MapKit
import GPSCore

struct LocationMapView: View {
    let fix: LocationFix?
    @State private var position: MapCameraPosition = .automatic

    var body: some View {
        Map(position: $position, interactionModes: [.pan, .zoom, .rotate]) {
            if let fix {
                Marker("최근 GPS 위치", coordinate: coordinate(fix))
                    .tint(.blue)
            }
        }
        .mapStyle(.standard(pointsOfInterest: .excludingAll))
        .mapControls { MapCompass(); MapScaleView() }
        .frame(height: 260)
        .accessibilityIdentifier("gps.map")
        .overlay(alignment: .topLeading) {
            if fix == nil {
                Label("위치 수신 대기", systemImage: "location")
                    .font(.callout)
                    .padding(10)
                    .background(.regularMaterial, in: RoundedRectangle(cornerRadius: 12))
                    .padding(12)
            }
        }
        .overlay(alignment: .topTrailing) {
            Button {
                if let fix { recenter(on: fix) }
            } label: {
                Image(systemName: "location.fill")
                    .padding(12)
                    .background(.regularMaterial, in: Circle())
            }
            .buttonStyle(.plain)
            .accessibilityLabel("현재 GPS 위치로 이동")
            .accessibilityIdentifier("gps.map.recenter")
            .disabled(fix == nil)
            .padding(12)
        }
        .onChange(of: fix, initial: true) { _, newFix in
            // Respect manual panning; recenter restores automatic following.
            if let newFix, !position.positionedByUser { recenter(on: newFix) }
        }
    }

    private func coordinate(_ fix: LocationFix) -> CLLocationCoordinate2D {
        CLLocationCoordinate2D(latitude: fix.latitude, longitude: fix.longitude)
    }

    private func recenter(on fix: LocationFix) {
        position = .region(MKCoordinateRegion(center: coordinate(fix),
            latitudinalMeters: 700, longitudinalMeters: 700))
    }
}

/// The map and values share one Core Location sample. TEST Mesh packets never replace it.
struct LocationReadout: View {
    let fix: LocationFix?
    let receiving: Bool

    var body: some View {
        #if targetEnvironment(simulator)
        Label("SIMULATOR · 모의 GPS", systemImage: "iphone.gen3")
            .font(.caption).foregroundStyle(.secondary)
        #endif
        if let fix {
            LabeledContent("위도", value: String(format: "%.7f", fix.latitude))
                .monospacedDigit().accessibilityIdentifier("gps.latitude")
            LabeledContent("경도", value: String(format: "%.7f", fix.longitude))
                .monospacedDigit().accessibilityIdentifier("gps.longitude")
            LabeledContent("정확도", value: String(format: "± %.1f m", fix.accuracy))
                .monospacedDigit()
            LabeledContent("마지막 갱신") { Text(Date(timeIntervalSince1970: fix.measuredAt), style: .time) }
            TimelineView(.periodic(from: .now, by: 1)) { context in
                let age = max(0, context.date.timeIntervalSince1970 - fix.measuredAt)
                LabeledContent("수신 상태", value: !receiving ? "일시 중지 · 마지막 위치" :
                    (age > 5 ? String(format: "%.0f초 전 · 새 위치 대기", age) : "GPS 수신 중"))
                    .foregroundStyle(age > 5 || !receiving ? .secondary : .primary)
            }
            if fix.accuracy > 50 {
                Text("대략적인 위치입니다. 정확도가 50m 이내일 때만 Mesh 전송에 사용합니다.")
                    .font(.footnote).foregroundStyle(.orange)
            }
        } else {
            ContentUnavailableView("GPS 위치 대기", systemImage: "location",
                description: Text(receiving ? "위치를 받으면 좌표와 지도 핀이 함께 표시됩니다." : "위치 권한을 허용하면 Mesh 연결 없이 확인할 수 있습니다."))
        }
    }
}

#Preview("GPS 수신 값") {
    Form {
        LocationReadout(fix: LocationFix(latitude: 37.5665, longitude: 126.978,
            accuracy: 5, measuredAt: Date().timeIntervalSince1970), receiving: true)
    }
}

#Preview("GPS 권한 대기") {
    Form { LocationReadout(fix: nil, receiving: false) }
}
