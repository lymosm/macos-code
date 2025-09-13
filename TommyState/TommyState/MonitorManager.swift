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
        timer?.schedule(deadline: .now(), repeating: 2.0) // 每秒刷新
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
        var newFanRPM: Int?
            // try multiple fan indices (0,1) until find valid
            for i in 0..<4 {
                if let rpm = SMCWrapper.shared.getFanRPM(index: i) {
                    newFanRPM = rpm
                    break
                }
            }

            // If we couldn't read, fallback to previous simulation
            let previousFan = self.fanRPM ?? 2000
            let newFan: Int
            if let real = newFanRPM {
                newFan = real
            } else {
                let fanDelta = Int.random(in: -120...120)
                // newFan = max(0, previousFan + fanDelta)
                newFan = 0
            }

        let previousGPU = self.gpuUsagePercent ?? 12.0

        // 平滑随机波动（演示用）
        let fanDelta = Int.random(in: -120...120)
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
