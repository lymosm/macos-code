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
