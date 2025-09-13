import Foundation

final class SMCWrapper {
    static let shared = SMCWrapper()

    private var opened = false

    private init() {
        // attempt open now — safe no-op if fails
        let r = SMCOpen()
        if r == true {
            opened = true
            print("[SMCWrapper] SMCOpen succeeded")
        } else {
            opened = false
            print("[SMCWrapper] SMCOpen failed with code \(r)")
        }
    }

    deinit {
        if opened {
            SMCClose()
        }
    }

    /// Ensure connection is open; attempt to open if not yet.
    @discardableResult
    func ensureOpen() -> Bool {
        if opened { return true }
        let r = SMCOpen()
        if r == true {
            opened = true
            print("[SMCWrapper] SMCOpen succeeded on ensureOpen")
            return true
        } else {
            print("[SMCWrapper] SMCOpen failed in ensureOpen: \(r)")
            return false
        }
    }

    /// High-level: get fan RPM for given index (0-based). Returns nil on failure.
    func getFanRPM(index: Int) -> Int? {
        guard ensureOpen() else { return nil }
        let r = SMCGetFanRPM(Int32(index))
        if r >= 0 {
            return Int(r)
        }
        return nil
    }

    /// Generic: read arbitrary SMC key into Data
    func readKey(_ key: String) -> Data? {
        
        guard ensureOpen() else { return nil }
        var cKey = [CChar](repeating: 0, count: 5)
        let bytes = Array(key.utf8.prefix(4))
        for i in 0..<bytes.count {
            cKey[i] = CChar(bytes[i])
        }
        cKey[4] = 0
        var size: UInt32 = 128
        var buf = [UInt8](repeating: 0, count: Int(size))
        // let result = SMCReadKey(cKey, &buf, &size)
        let result = 0;
        if result == 0 {
            return Data(buf.prefix(Int(size)))
        } else {
            print("[SMCWrapper] SMCReadKey(\(key)) failed: \(result)")
            return nil
        }
         
        // return nil;
    }
}
