import SwiftUI
import AppKit

@main
struct TommyState: App {
    // 挂接 AppDelegate
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    var body: some Scene {
        WindowGroup("TommyState Main") {
            ContentView()
                .environmentObject(MonitorManager.shared)
                .frame(minWidth: 520, minHeight: 300)
        }
        .handlesExternalEvents(matching: ["main"])
    }
}

// MARK: - AppDelegate 负责启动时初始化
class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        Logger.log("applicationDidFinishLaunching")

        // 隐藏 Dock 图标（可选，看你需不需要）
        NSApp.setActivationPolicy(.accessory)

        // 启动监控
        MonitorManager.shared.startUpdating()

        // 初始化菜单栏
        StatusBarController.shared.setup(monitor: MonitorManager.shared)

        Logger.log("StatusBarController.setup done")
    }
}

// MARK: - Logger 保持不变
struct Logger {
    static let path = ("~/Library/Logs/TommyState.log" as NSString).expandingTildeInPath
    static let queue = DispatchQueue(label: "tommy.tommystate.logger")

    static func log(_ items: Any...) {
        let s = items.map { "\($0)" }.joined(separator: " ")
        let ts = ISO8601DateFormatter().string(from: Date())
        let line = "[\(ts)] \(s)\n"
        queue.async {
            if let data = line.data(using: .utf8) {
                if FileManager.default.fileExists(atPath: path) {
                    if let fh = FileHandle(forWritingAtPath: path) {
                        fh.seekToEndOfFile()
                        fh.write(data)
                        fh.closeFile()
                        return
                    }
                }
                try? data.write(to: URL(fileURLWithPath: path), options: .atomic)
            }
        }
    }
}

