import SwiftUI

struct ContentView: View {
    @EnvironmentObject var monitor: MonitorManager

    // Which tab is selected
    @State private var selectedTab: PanelView.Tab = .fan

    // listen for status bar clicks
    init() {
        // nothing specific here
    }

    var body: some View {
        PanelView(selectedTab: $selectedTab)
            .environmentObject(monitor)
            .onReceive(NotificationCenter.default.publisher(for: .TommyStateOpenPanel)) { note in
                    if let tab = note.object as? PanelView.Tab {
                        print("[ContentView] 收到通知，切换到 \(tab)")
                        selectedTab = tab
                }
            }
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView()
            .environmentObject(MonitorManager.shared)
            .frame(width: 640, height: 360)
    }
}
