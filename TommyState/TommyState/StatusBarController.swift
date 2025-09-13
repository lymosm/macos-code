import Cocoa
import SwiftUI

/// 管理菜单栏的 Fan / GPU 图标（两个独立的 NSStatusItem）
/// 点击图标会激活主窗口（WindowGroup 的 ContentView）
final class StatusBarController {
    static let shared = StatusBarController()

    private var fanItem: NSStatusItem?
    private var gpuItem: NSStatusItem?

    private weak var monitor: MonitorManager?

    private init() {}

    func setup(monitor: MonitorManager) {
        self.monitor = monitor
        monitor.statusUpdate = { [weak self] in
            DispatchQueue.main.async {
                self?.refresh()
            }
        }
        createOrUpdateStatusItems()
    }

    private func createOrUpdateStatusItems() {
        // Fan item
        if fanItem == nil {
            fanItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
            fanItem?.button?.action = #selector(fanClicked)
            fanItem?.button?.target = self
        }
        // GPU item
        if gpuItem == nil {
            gpuItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
            gpuItem?.button?.action = #selector(gpuClicked)
            gpuItem?.button?.target = self
        }
        refresh()
    }

    private func refresh() {
        guard let monitor = monitor else { return }

        // Fan
        if monitor.showFanInMenu {
            if fanItem == nil { createOrUpdateStatusItems() }
            if let btn = fanItem?.button {
                let displayText: String
                if let rpm = monitor.fanRPM { displayText = "\(rpm)" }
                else { displayText = "-- RPM" }
                // use SF Symbol "fanblades" (available macOS 11+)
                if let img = NSImage(systemSymbolName: "fanblades", accessibilityDescription: "Fan") {
                    img.isTemplate = true
                    btn.image = img
                }
                btn.title = " \(displayText)"
                btn.toolTip = "Fan RPM: \(monitor.fanRPM.map { "\($0)" } ?? "--")"
            }
            fanItem?.isVisible = true
        } else {
            fanItem?.isVisible = false
        }

        // GPU
        if monitor.showGPUInMenu {
            if gpuItem == nil { createOrUpdateStatusItems() }
            if let btn = gpuItem?.button {
                let displayText: String
                if let gpu = monitor.gpuUsagePercent { displayText = String(format: "%.0f%%", gpu) }
                else { displayText = "--%" }
                if let img = NSImage(systemSymbolName: "sparkles.tv", accessibilityDescription: "GPU") {
                    img.isTemplate = true
                    btn.image = img
                }
                btn.title = " \(displayText)"
                btn.toolTip = "GPU: \(monitor.gpuUsagePercent.map { String(format: "%.1f%%", $0) } ?? "--")"
            }
            gpuItem?.isVisible = true
        } else {
            gpuItem?.isVisible = false
        }
    }

    @objc private func fanClicked() {
        showMainWindow(selectTab: .fan)
    }

    @objc private func gpuClicked() {
        showMainWindow(selectTab: .gpu)
    }

    private func showMainWindow(selectTab: PanelView.Tab) {
        // 激活 app 并展示主 WindowGroup（ContentView），并把侧栏选为相应 tab
        DispatchQueue.main.async {
                    print("[StatusBarController] 点击 \(selectTab) 图标，准备展示主窗口")
                    NSApp.activate(ignoringOtherApps: true)

                    // 找一个非 NSStatusBarWindow 的 SwiftUI 主窗口
                    if let window = NSApp.windows.first(where: { String(describing: type(of: $0)) != "NSStatusBarWindow" }) {
                        print("[StatusBarController] 找到现有 SwiftUI 窗口，bringToFront")
                        window.makeKeyAndOrderFront(nil)
                    } else {
                        print("[StatusBarController] 没有找到主窗口，尝试通过 URL Scheme 创建")
                        if let url = URL(string: "TommyState://main") {
                            NSWorkspace.shared.open(url)
                        }
                    }

                    NotificationCenter.default.post(name: .TommyStateOpenPanel, object: selectTab)
                    print("[StatusBarController] 已发送 TommyStateOpenPanel 通知")
                }
    }
}

extension Notification.Name {
    static let TommyStateOpenPanel = Notification.Name("TommyStateOpenPanel")
}
