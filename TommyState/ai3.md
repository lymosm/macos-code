1. TommyState.app 中嵌入了：Contents/Library/LaunchServices/tommyTool (command tool) ，文件确认存在
2. tommyTool 主要是用于写入文件：Library/LaunchDaemons/tommy.tommystate.plist 让TommyState.app自启动
3. 用的是SMJobBless方式，代码如下：
func installHelper() {
    var cfError: Unmanaged<CFError>?
    let helperID = "tommy.TommyTool"
    print("Helper install ing")
    if SMJobBless(kSMDomainSystemLaunchd, helperID as CFString, nil, &cfError) {
        print("✅ Helper installed successfully")
    } else {
        if let error = cfError?.takeRetainedValue() {
            print("❌ SMJobBless failed:", error)
        } else {
            print("❌ SMJobBless failed with unknown error")
        }
    }
    print("Helper install end")
}
4. TommyState.app 的Info.plist:
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleURLTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeRole</key>
            <string>Editor</string>
            <key>CFBundleURLName</key>
            <string>TommyStateURL</string>
            <key>CFBundleURLSchemes</key>
            <array>
                <string>TommyState</string>
            </array>
        </dict>
    </array>
    <key>SMPrivilegedExecutables</key>
    <dict>
        <key>tommy.TommyTool</key>
        <string>Contents/Library/LaunchServices/tommyTool</string>
    </dict>


</dict>
</plist>

5. tommyTool的Info.plist:
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>tommy.TommyTool</string>
    <key>CFBundleName</key>
    <string>tommyTool</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleExecutable</key>
    <string>tommyTool</string>
    <!-- 确保 helper 不会显示 Dock 图标或菜单栏图标 -->
    <key>LSBackgroundOnly</key>
    <true/>
</dict>
</plist>

6.点击调用：
            // launch daemon
            Toggle("开机启动 (Daemon)", isOn: Binding(
                get: {
                    FileManager.default.fileExists(atPath: "/Library/LaunchDaemons/tommy.tommystate.plist")
                },
                set: { newValue, _ in
                    if newValue {
                        installHelper()
                    } else {
                        removeHelper()
                    }
                }
            ))
            .toggleStyle(.switch)
            
7. 现在报错日志：
Helper install ing
❌ SMJobBless failed: Error Domain=CFErrorDomainLaunchd Code=4 "(null)"
Helper install end
请问下怎么解决啊？



<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleIdentifier</key>
    <string>tommy.TommyTool</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleExecutable</key>
    <string>TommyTool</string>
    <!-- 确保 helper 不会显示 Dock 图标或菜单栏图标 -->
    <key>LSBackgroundOnly</key>
    <true/>

    <key>SMAuthorizedClients</key>
    <array>
        <string>identifier "tommy.TommyState" and anchor apple generic and certificate leaf[subject.CN] = "Apple Development: lymosm@163.com (D4N9J8SB5B)" and certificate 1[field.1.2.840.113635.100.6.2.1] /* exists */</string>
    </array>
</dict>
</plist>
