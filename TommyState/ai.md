ContentView.swift:
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

MonitorManager.swift:
import Foundation
import Combine

/// MonitorManager: 负责每秒刷新数据、保存开关、提供给 UI 使用
final class MonitorManager: ObservableObject {
    static let shared = MonitorManager()

    // controls whether icons show in menu bar
    @Published var showFanInMenu: Bool = true {
        didSet { statusUpdate?() }
    }
    @Published var showGPUInMenu: Bool = true {
        didSet { statusUpdate?() }
    }

    // 感测到的值（UI 使用）
    @Published private(set) var fanRPM: Int? = nil       // nil 表示未知/不可用
    @Published private(set) var gpuUsagePercent: Double? = nil

    // update timer
    private var timer: DispatchSourceTimer?
    private let queue = DispatchQueue(label: "com.tommystate.monitor", qos: .utility)

    // status update hook (for status bar controller to refresh icons quickly)
    var statusUpdate: (() -> Void)?

    private init() {
        // 如果你想持久化 last state, 在此从 UserDefaults 加载 showFanInMenu / showGPUInMenu
        let ud = UserDefaults.standard
        showFanInMenu = ud.bool(forKey: "showFanInMenu") // default false if not set, we'll set default true:
        if ud.object(forKey: "showFanInMenu") == nil { showFanInMenu = true }
        showGPUInMenu = ud.bool(forKey: "showGPUInMenu")
        if ud.object(forKey: "showGPUInMenu") == nil { showGPUInMenu = true }
    }

    deinit {
        stopUpdating()
    }

    func startUpdating() {
        // 如果已经在运行，先停止
        stopUpdating()

        timer = DispatchSource.makeTimerSource(queue: queue)
        timer?.schedule(deadline: .now(), repeating: 1.0) // 每秒刷新
        timer?.setEventHandler { [weak self] in
            guard let self = self else { return }
            self.fetchSensorValues()
        }
        timer?.resume()
    }

    func stopUpdating() {
        timer?.cancel()
        timer = nil
    }

    // MARK: - 获取传感器的占位/真实实现点
    private func fetchSensorValues() {
        // ====== 占位实现（用于 UI 开发与演示） ======
        // 我们在无法读取真实 SMC/IOKit 时，先用模拟值（平滑变化）
        // 真实环境下请在下面的 "真实实现示例" 区域里接入 IOKit/SMC/powermetrics 等代码/工具。

        let previousFan = self.fanRPM ?? 2000
        let previousGPU = self.gpuUsagePercent ?? 12.0

        // 平滑随机波动（演示用）
        let fanDelta = Int.random(in: -120...120)
        let newFan = max(0, previousFan + fanDelta)
        let gpuDelta = Double.random(in: -6...6)
        let newGPU = min(max(0.0, previousGPU + gpuDelta), 100.0)

        DispatchQueue.main.async {
            self.fanRPM = newFan
            self.gpuUsagePercent = newGPU
            // persist user choices
            let ud = UserDefaults.standard
            ud.set(self.showFanInMenu, forKey: "showFanInMenu")
            ud.set(self.showGPUInMenu, forKey: "showGPUInMenu")
            // notify status bar
            self.statusUpdate?()
        }

        // ====== 真实实现示例（说明） ======
        // 在这里你可以替换为：
        //  1) 调用一个本地 helper（使用 SMC 或 powermetrics）并解析输出（需要 root 或权限）。
        //  2) 使用 IOKit / AppleSMC 的 C 接口来读取 SMC keys（例如 F0Ac 等） —— 需要把 C 源和桥接文件加到项目并实现。
        //  3) 使用 IOKit 查询 GPU driver 或调用 Metal/perf counters（通常不可行或受限）。
        //
        // 如果你需要，我可以为你提供一个基于开源 SMC 例子（C 源 + Swift 桥接）的实现方法，但那会更长也更复杂。
    }

    // 以下提供外部触发强制刷新接口（比如在开发时点击按钮）
    func forceRefreshNow() {
        queue.async { [weak self] in self?.fetchSensorValues() }
    }
}

PanelView.swift:
import SwiftUI

struct PanelView: View {
    enum Tab: String, CaseIterable, Identifiable {
        case fan = "风扇"
        case gpu = "GPU"
        var id: String { rawValue }
    }

    @EnvironmentObject var monitor: MonitorManager
    @Binding var selectedTab: Tab

    var body: some View {
        HStack(spacing: 0) {
            sidebarView()
            Divider()
            mainPanelView()
        }
        .frame(minWidth: 520, minHeight: 300)
    }
}

// MARK: - 子视图
extension PanelView {
    @ViewBuilder
    private func sidebarView() -> some View {
        VStack(spacing: 8) {
            Text("TommyState")
                .font(.title2.bold())
                .padding(.top, 12)

            VStack(spacing: 6) {
                ForEach(Tab.allCases) { t in
                    tabButton(for: t)
                }
            }
            .padding(.vertical, 6)

            Spacer()
        }
        .frame(minWidth: 140, maxWidth: 180)
        .padding(.leading, 8)
        .background(
            VisualEffectView(
                material: NSVisualEffectView.Material.sidebar,
                blendingMode: NSVisualEffectView.BlendingMode.withinWindow
            )
        )
    }

    @ViewBuilder
    private func mainPanelView() -> some View {
        VStack(alignment: .leading, spacing: 16) {
            headerView()

            if selectedTab == .fan {
                FanCard(fanRPM: monitor.fanRPM)
            }
            if selectedTab == .gpu {
                GPUCard(gpuUsage: monitor.gpuUsagePercent)
            }

            Spacer()
            footerView()
        }
        .padding(16)
        .frame(minWidth: 320)
    }

    private func headerView() -> some View {
        HStack {
            Text(selectedTab.rawValue)
                .font(.title3.bold())
            Spacer()
            Toggle(isOn: bindingForTab(selectedTab)) {
                EmptyView()
            }
            .toggleStyle(SwitchToggleStyle())
        }
    }

    private func footerView() -> some View {
        HStack {
            Button("立即刷新") {
                monitor.forceRefreshNow()
            }
            Spacer()
            Text("TommyState • demo")
                .font(.caption)
                .foregroundStyle(.secondary)
        }
    }

    private func tabButton(for tab: Tab) -> some View {
        Button(action: { selectedTab = tab }) {
            HStack {
                Image(systemName: iconName(for: tab))
                    .font(.system(size: 16))
                    .frame(width: 20)
                Text(tab.rawValue)
                    .font(.system(size: 14, weight: selectedTab == tab ? .semibold : .regular))
                Spacer()
                if indicator(for: tab) {
                    Circle()
                        .frame(width: 8, height: 8)
                        .foregroundStyle(.primary)
                }
            }
            .padding(.vertical, 8)
            .padding(.horizontal, 10)
            // ✅ 改用 overlay 避免类型不匹配
            .overlay(
                RoundedRectangle(cornerRadius: 8)
                    .fill(Color.primary.opacity(selectedTab == tab ? 0.12 : 0))
            )
        }
        .buttonStyle(PlainButtonStyle())
    }

    // 工具方法
    private func iconName(for tab: Tab) -> String {
        tab == .fan ? "fanblades" : "sparkles.tv"
    }

    private func indicator(for tab: Tab) -> Bool {
        tab == .fan ? monitor.showFanInMenu : monitor.showGPUInMenu
    }

    private func bindingForTab(_ tab: Tab) -> Binding<Bool> {
        tab == .fan ? $monitor.showFanInMenu : $monitor.showGPUInMenu
    }
}

// MARK: - Fan 卡片
struct FanCard: View {
    var fanRPM: Int?

    var body: some View {
        InfoCard {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Image(systemName: "fanblades").font(.largeTitle)
                    VStack(alignment: .leading) {
                        Text("风扇转速").font(.headline)
                        if let rpm = fanRPM {
                            Text("\(rpm) RPM")
                                .font(.title)
                                .monospacedDigit()
                        } else {
                            Text("无法获取").foregroundStyle(.secondary)
                        }
                    }
                    Spacer()
                }
                ProgressView(value: progress, total: 1.0)
                Text("每秒刷新 • 可配置 IOKit/SMC 接入")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }

    private var progress: Double {
        guard let rpm = fanRPM else { return 0 }
        return min(max(Double(rpm) / 6000.0, 0.0), 1.0)
    }
}

// MARK: - GPU 卡片
struct GPUCard: View {
    var gpuUsage: Double?

    var body: some View {
        InfoCard {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Image(systemName: "sparkles.tv").font(.largeTitle)
                    VStack(alignment: .leading) {
                        Text("GPU 使用率").font(.headline)
                        if let usage = gpuUsage {
                            Text(String(format: "%.0f %%", usage))
                                .font(.title)
                                .monospacedDigit()
                        } else {
                            Text("无法获取").foregroundStyle(.secondary)
                        }
                    }
                    Spacer()
                }
                ProgressView(value: (gpuUsage ?? 0) / 100.0, total: 1.0)
                Text("每秒刷新 • 需要 powermetrics 或 GPU driver 数据")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
    }
}

// MARK: - 通用 Info 卡片容器
struct InfoCard<Content: View>: View {
    @ViewBuilder var content: Content

    var body: some View {
        VStack { content }
            .padding()
            .background(
                RoundedRectangle(cornerRadius: 12)
                    .fill(Color(NSColor.windowBackgroundColor).opacity(0.95))
            )
            .shadow(radius: 2, y: 1)
    }
}

StatusBarController.swift:
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
                if let rpm = monitor.fanRPM { displayText = "\(rpm) RPM" }
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
            NSApp.activate(ignoringOtherApps: true)
            // set a notification or user defaults so the ContentView knows which tab to select
            NotificationCenter.default.post(name: .TommyStateOpenPanel, object: selectTab)
        }
    }
}

extension Notification.Name {
    static let TommyStateOpenPanel = Notification.Name("TommyStateOpenPanel")
}

TommyStateApp.swift:
import SwiftUI
import AppKit

@main
struct TommyState: App {
    // Monitor 实例在应用全局共享
    @StateObject private var monitor = MonitorManager.shared
    // 管理菜单栏 item
    private let statusController = StatusBarController.shared

    init() {
        // 作为菜单栏工具应用，隐藏 Dock 图标（可选）
        NSApp.setActivationPolicy(.accessory)

        // 启动监控（不会立即调用复杂权限的代码，只是启动计时器）
        monitor.startUpdating()

        // 初始化 status items
        statusController.setup(monitor: monitor)
    }

    var body: some Scene {
        WindowGroup {
            // 这里放置主 panel（当点击 menu item 时会弹出 popover/window）
            ContentView()
                .environmentObject(monitor)
                .frame(minWidth: 520, minHeight: 300)
        }
        .handlesExternalEvents(matching: Set(arrayLiteral: "*"))
        .commands {
            // 可选命令
        }
       

    }
}

Utilities.swift:
import Foundation
import AppKit

extension NSImage {
    /// Helper to create template SF Symbol images safely
    static func sfSymbol(_ name: String, accessibility: String? = nil) -> NSImage? {
        if #available(macOS 11.0, *) {
            let img = NSImage(systemSymbolName: name, accessibilityDescription: accessibility)
            img?.isTemplate = true
            return img
        } else {
            return nil
        }
    }
}

VisualEffectView.swift:
import SwiftUI

struct VisualEffectView: NSViewRepresentable {
    var material: NSVisualEffectView.Material
    var blendingMode: NSVisualEffectView.BlendingMode

    func makeNSView(context: Context) -> NSVisualEffectView {
        let view = NSVisualEffectView()
        view.material = material
        view.blendingMode = blendingMode
        view.state = .active
        return view
    }

    func updateNSView(_ nsView: NSVisualEffectView, context: Context) {
        nsView.material = material
        nsView.blendingMode = blendingMode
    }
}

以上是各个文件的代码，能编译成功，但是运行报错：TommyState/TommyStateApp.swift:13: Fatal error: Unexpectedly found nil while implicitly unwrapping an Optional value

