import XCTest
import UIKit

/// Field-specific layouts like the system keyboard (email, URL, web search, number pads), rendered for a look.
final class KeyboardLayoutTests: XCTestCase {
    private func render(_ type: UIKeyboardType, layer: KeyboardView.Layer = .letters, _ name: String) -> KeyboardView {
        let kb = KeyboardView(frame: CGRect(x: 0, y: 0, width: 402, height: KeyboardView.keyboardHeight(compactHeight: false)))
        kb.fieldType = type
        kb.layer_ = layer
        kb.backgroundColor = UIColor(red: 0.82, green: 0.83, blue: 0.86, alpha: 1)
        kb.layoutIfNeeded()
        let img = UIGraphicsImageRenderer(bounds: kb.bounds).image { ctx in kb.layer.render(in: ctx.cgContext) }
        try? FileManager.default.createDirectory(atPath: "/tmp/claude-501/layouts", withIntermediateDirectories: true)
        try? img.pngData()?.write(to: URL(fileURLWithPath: "/tmp/claude-501/layouts/\(name).png"))
        return kb
    }
    private func titles(_ kb: KeyboardView) -> [String] { kb.subviews.compactMap { ($0 as? KeyCap)?.label.text } }

    func testEmailHasAtAndDot() { let t = titles(render(.emailAddress, "email")); XCTAssertTrue(t.contains("@") && t.contains(".")) }
    func testURLHasSlashAndDotCom() { let t = titles(render(.URL, "url")); XCTAssertTrue(t.contains("/") && t.contains(".com")) }
    func testWebSearchHasDot() { XCTAssertTrue(titles(render(.webSearch, "websearch")).contains(".")) }
    func testNumberPad() {
        let kb = render(.numberPad, layer: .numpad, "numberpad")
        XCTAssertEqual(Set(titles(kb).filter { !$0.isEmpty }), Set("1234567890".map(String.init)))
    }
    func testDecimalAndPhonePads() {
        XCTAssertTrue(titles(render(.decimalPad, layer: .numpad, "decimalpad")).contains(Locale.current.decimalSeparator ?? "."))
        XCTAssertTrue(titles(render(.phonePad, layer: .numpad, "phonepad")).contains("+"))
    }
    func testDefaultUnchanged() { let t = titles(render(.default, "default")); XCTAssertFalse(t.contains("@")) }
}

/// Design proposals: the VI/EN key in the top-left corner of the suggestion strip (Tony). Renders only.
final class LanguageKeyProposalTests: XCTestCase {
    func testRenderProposals() {
        KeyboardView.barOn = true
        for dark in [false, true] {
            for style in 0...3 {
                KeyboardView.langStyle = style
                let kb = KeyboardView(frame: CGRect(x: 0, y: 0, width: 402, height: KeyboardView.keyboardHeight(compactHeight: false)))
                kb.dark = dark
                kb.languageShort = "VI"; kb.languageTitle = "Tiếng Việt"
                kb.backgroundColor = dark ? UIColor(white: 0.17, alpha: 1) : UIColor(red: 0.82, green: 0.83, blue: 0.86, alpha: 1)
                kb.suggestions = [Suggestion(text: "tooi", quoted: true), Suggestion(text: "tôi", highlighted: true), Suggestion(text: "tối")]
                kb.layoutIfNeeded()
                let img = UIGraphicsImageRenderer(bounds: kb.bounds).image { ctx in kb.layer.render(in: ctx.cgContext) }
                try? img.pngData()?.write(to: URL(fileURLWithPath: "/tmp/claude-501/layouts/lang_\(style)_\(dark ? "dark" : "light").png"))
            }
        }
        KeyboardView.langStyle = 1
    }

    func testPillStates() {
        KeyboardView.barOn = true; KeyboardView.langStyle = 1
        for (lang, title) in [("VI", "Tiếng Việt"), ("EN", "English")] {
            let kb = KeyboardView(frame: CGRect(x: 0, y: 0, width: 402, height: KeyboardView.keyboardHeight(compactHeight: false)))
            kb.languageShort = lang; kb.languageTitle = title
            kb.backgroundColor = UIColor(red: 0.82, green: 0.83, blue: 0.86, alpha: 1)
            kb.layoutIfNeeded()
            let img = UIGraphicsImageRenderer(bounds: kb.bounds).image { ctx in kb.layer.render(in: ctx.cgContext) }
            try? img.pngData()?.write(to: URL(fileURLWithPath: "/tmp/claude-501/layouts/pill_\(lang).png"))
            let caps = kb.subviews.compactMap { $0 as? KeyCap }
            XCTAssertTrue(caps.contains { $0.frame.minY < 40 && ($0.label.attributedText?.string == lang) }, "the pill sits in the strip's corner")
        }
        KeyboardView.barOn = false
        let kb = KeyboardView(frame: CGRect(x: 0, y: 0, width: 402, height: KeyboardView.keyboardHeight(compactHeight: false)))
        kb.layoutIfNeeded()
        XCTAssertTrue(kb.subviews.compactMap { $0 as? KeyCap }.contains { $0.label.text == "VI" && $0.frame.minY > 150 }, "no strip: back in the bottom row")
    }
}


/// Key frames vs the iOS 26.5 system keyboard, measured from simulator screenshots of iPhone 17 Pro Max (440 pt).
final class SystemKeyboardGeometryTests: XCTestCase {
    func testMatchesSystemKeyboardAt440() throws {
        guard KeyboardView.tallKeys else { throw XCTSkip("needs a Pro Max-size simulator") }
        KeyboardView.barOn = true; KeyboardView.langStyle = 1
        let kb = KeyboardView(frame: CGRect(x: 0, y: 0, width: 440, height: KeyboardView.keyboardHeight(compactHeight: false)))
        kb.layoutIfNeeded()
        let caps = kb.subviews.compactMap { $0 as? KeyCap }
        func frame(_ label: String) -> CGRect? { caps.first { $0.label.text == label }?.frame }
        let q = try XCTUnwrap(frame("q")), a = try XCTUnwrap(frame("a")), z = try XCTUnwrap(frame("z")), p = try XCTUnwrap(frame("p"))
        let top = q.minY
        func near(_ v: CGFloat, _ want: CGFloat, _ what: String) { XCTAssertEqual(v, want, accuracy: 0.5, what) }
        near(q.minX, 6.67, "q x"); near(q.width, 37.33, "letter width"); near(q.height, 45, "key height")
        near(p.maxX, 433.67, "p right edge")
        near(a.minX, 28.0, "a x"); near(a.minY - top, 56, "row pitch")
        near(z.minX, 71.33, "z x"); near(z.minY - top, 112, "row 3")
        // row 3 sides: shift 6.67 w 50.0, delete 383.33 w 50.33; bottom row: 123 w 48 at 6.67, return 331.33 w 102.33
        let row3 = caps.filter { abs($0.frame.minY - z.minY) < 1 }.sorted { $0.frame.minX < $1.frame.minX }
        near(row3.first!.frame.width, 50, "shift width"); near(row3.last!.frame.minX, 383.33, "delete x")
        let bottom = caps.filter { abs($0.frame.minY - (top + 168)) < 1 }.sorted { $0.frame.minX < $1.frame.minX }
        near(bottom.first!.frame.width, 48, "123 width"); near(bottom.last!.frame.minX, 331.33, "return x"); near(bottom.last!.frame.width, 102.33, "return width")
    }
}

/// The brand on the right of an empty suggestion strip (Tony). Renders only.
final class StripBrandTests: XCTestCase {
    func testRenderEmptyAndFullStrip() {
        KeyboardView.barOn = true
        KeyboardView.langStyle = 1
        for dark in [false, true] {
            for (name, s) in [("empty", [Suggestion]()), ("words", [Suggestion(text: "tooi", quoted: true), Suggestion(text: "tôi", highlighted: true), Suggestion(text: "tối")])] {
                let kb = KeyboardView(frame: CGRect(x: 0, y: 0, width: 402, height: KeyboardView.keyboardHeight(compactHeight: false)))
                kb.dark = dark
                kb.languageShort = "VI"; kb.languageTitle = "Tiếng Việt"
                kb.backgroundColor = dark ? UIColor(white: 0.17, alpha: 1) : UIColor(red: 0.82, green: 0.83, blue: 0.86, alpha: 1)
                kb.suggestions = s
                kb.layoutIfNeeded()
                let img = UIGraphicsImageRenderer(bounds: kb.bounds).image { ctx in kb.layer.render(in: ctx.cgContext) }
                try? FileManager.default.createDirectory(atPath: "/tmp/claude-501/layouts", withIntermediateDirectories: true)
                try? img.pngData()?.write(to: URL(fileURLWithPath: "/tmp/claude-501/layouts/brand_\(name)_\(dark ? "dark" : "light").png"))
            }
        }
    }
}
