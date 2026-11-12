import AppKit
import Carbon

/// Keyboards (macOS input sources) the user has added to Vikey's "Bàn phím" tab, each with a switch: does Vikey type
/// Vietnamese on it. A keyboard that isn't in the list is left to Vikey's own guess
/// (VikeyCore.inputSourceIsIgnoredByDefault: Japanese, Chinese... keyboards are ignored, Latin ones are used).
/// The list is a UserDefaults dictionary (PrefKey.keyboardOverrides: input source id -> 1 / 0) that VikeyCore.mm reads
/// in VKUpdateLayoutCompat.
enum KeyboardLayouts {
    struct Source: Identifiable {
        let id: String
        let name: String
        let isEnabledInMac: Bool   // turned on in System Settings > Keyboard > Input Sources
    }

    /// Every keyboard macOS knows (installed, enabled or not), sorted by name.
    static func installed() -> [Source] {
        let filter = [kTISPropertyInputSourceIsSelectCapable as String: true,
                      kTISPropertyInputSourceCategory as String: kTISCategoryKeyboardInputSource as String] as CFDictionary
        guard let list = TISCreateInputSourceList(filter, true)?.takeRetainedValue() as? [TISInputSource] else { return [] }
        // layouts, modes of an input method (Hiragana...) and input methods without modes; not the synthetic Roman mode
        let types: Set<String> = [kTISTypeKeyboardLayout as String, kTISTypeKeyboardInputMode as String,
                                  kTISTypeKeyboardInputMethodWithoutModes as String]
        var seen = Set<String>()
        return list.compactMap { src in
            guard let id = string(src, kTISPropertyInputSourceID), seen.insert(id).inserted else { return nil }
            guard let type = string(src, kTISPropertyInputSourceType), types.contains(type) else { return nil }
            let enabled = TISGetInputSourceProperty(src, kTISPropertyInputSourceIsEnabled).map {
                CFBooleanGetValue(Unmanaged<CFBoolean>.fromOpaque($0).takeUnretainedValue())
            } ?? false
            return Source(id: id, name: string(src, kTISPropertyLocalizedName) ?? id, isEnabledInMac: enabled)
        }
        .sorted { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
    }

    /// The saved list: input source id -> 1 (Vikey types) / 0 (Vikey stays out).
    static var overrides: [String: Int] {
        (UserDefaults.standard.dictionary(forKey: PrefKey.keyboardOverrides) as? [String: Int]) ?? [:]
    }

    /// Does Vikey type on a keyboard when the user hasn't chosen: Vikey's own guess.
    static func defaultOn(_ id: String) -> Bool { !VikeyCore.inputSourceIsIgnored(byDefault: id) }

    static func set(_ id: String, on: Bool) {
        var m = overrides
        m[id] = on ? 1 : 0
        save(m)
    }

    /// Adds a keyboard with Vikey's guess as its first state.
    static func add(_ id: String) {
        guard overrides[id] == nil else { return }
        set(id, on: defaultOn(id))
    }

    static func remove(_ id: String) {
        var m = overrides
        m.removeValue(forKey: id)
        save(m)
    }

    private static func save(_ m: [String: Int]) {
        UserDefaults.standard.set(m, forKey: PrefKey.keyboardOverrides)
        Preferences.apply()
    }

    private static func string(_ src: TISInputSource, _ key: CFString) -> String? {
        TISGetInputSourceProperty(src, key).map { Unmanaged<CFString>.fromOpaque($0).takeUnretainedValue() as String }
    }
}
