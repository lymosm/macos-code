// TommyStateApp.swift

import SwiftUI
import AppKit

@main
struct TommyState: App {
    // Monitor 实例在应用全局共享
    @StateObject private var monitor = MonitorManager.shared
    // 管理菜单栏 item
    private let statusController = StatusBarController.shared

    var body: some Scene {
        WindowGroup {
            // 这里放置主 panel（当点击 menu item 时会弹出 popover/window）
            ContentView()
                .environmentObject(monitor)
                .frame(minWidth: 520, minHeight: 300)
                .onAppear {
                    // ✅ Move setup logic here.
                    // This closure is executed after 'monitor' has been initialized.

                    // 作为菜单栏工具应用，隐藏 Dock 图标（可选）
                    NSApp.setActivationPolicy(.accessory)

                    // 启动监控
                    monitor.startUpdating()

                    // 初始化 status items
                    statusController.setup(monitor: monitor)
                }
        }
        .handlesExternalEvents(matching: Set(arrayLiteral: "*"))
        .commands {
            // 可选命令
        }
    }
}
