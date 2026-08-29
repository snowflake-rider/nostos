import SwiftUI

@main struct GPSMeshApp: App {
    @State private var model = AppModel()
    var body: some Scene { WindowGroup { RootView(model: model) } }
}
