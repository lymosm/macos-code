import Foundation

let plistPath = "/Library/LaunchDaemons/tommy.tommystate.plist"
let plist = """
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
                      "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>tommy.tommystate</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Applications/TommyState.app/Contents/MacOS/TommyState</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
</dict>
</plist>
"""

do {
    try plist.write(toFile: plistPath, atomically: true, encoding: .utf8)
    print("✅ Daemon plist installed at \(plistPath)")

    let task = Process()
    task.executableURL = URL(fileURLWithPath: "/bin/launchctl")
    task.arguments = ["load", plistPath]
    try task.run()
} catch {
    print("❌ Failed to install LaunchDaemon:", error)
}
