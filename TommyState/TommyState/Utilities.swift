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
