用swift语言，xcode 16.4开发一个macOS App，功能如下：
1. 监控风扇转速和GPU使用率，在菜单栏显示对应的Icon，动态展示，每秒刷新
2. 需要一个panel，左侧是tab,可以切换：风扇/GPU，右侧是对应的一个开关，关闭则对应的icon不在菜单栏显示
3. 界面尽量做到美观一点
4. 如果无法直接获取风扇转速，可以考虑对接IOKit
5. 项目名称：TommyState，创建项目后，Xcode 16.4自动创建的现有目录结构如下：
    TommyState.prj
        TommyState/
            Assets
            ContentView.swift
            TommyState.entitlements
            TommyState.swift
        TommyStateTests/
            TommyStateTests.swift
        TommyStateUITests/
            TommyStateUITests.swift
            TommyStateUITestsLaunchTests.swift
6. 请帮我写出每一个文件的完整代码和调试步骤



"-sectcreate __TEXT __info_plist $(SRCROOT)/TommyHelper/Info.plist"
