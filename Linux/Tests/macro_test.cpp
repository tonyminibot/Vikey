// Gõ tắt (D68): the table, the file format, auto caps and the English word tracker. No IBus needed.
#include "Macro.h"
#include "Engine.h"
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace vikey;
// the settings the engine reads (the platform layer defines them)
int vLanguage = 1, vSwitchKeyStatus = 0, vCheckSpelling = 1, vUseModernOrthography = 1, vRestoreIfWrongSpelling = 1,
    vFixRecommendBrowser = 1, vUseSmartSwitchKey = 1, vOtherLanguage = 1, vAutoSwitchByTyping = 0;
static int fails = 0;
static Uint16 K(char c) { static const Uint16 L[26] = {KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,KEY_Y,KEY_Z};
    return c == ' ' ? KEY_SPACE : L[c - 'a']; }
/// Types the keys through the Vietnamese engine and returns the word it says is on screen.
static std::string screenWord(const char* keys) {
    vKeyInit(); vLanguage = 1; startNewSession();
    for (const char* p = keys; *p; p++) vKeyHandleEvent(vKeyEvent::Keyboard, vKeyEventState::KeyDown, K(*p), 0, false);
    char32_t w[40]; int n = vTypingWordText(w, 40); return macro::toUtf8(std::u32string(w, w + n));
}
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static std::string expand(const std::string& w, bool caps) { std::u32string o; return macro::lookup(macro::fromUtf8(w), caps, o) ? macro::toUtf8(o) : "<none>"; }

int main() {
    macro::clear();
    CHECK(macro::set("tks", "cảm ơn"));
    CHECK(macro::set("đc", "được"));
    CHECK(macro::set("vn", "Việt Nam"));
    CHECK(!macro::set("", "x") && !macro::set("a b", "x") && !macro::set("ok", "") && !macro::set("a:b", "x"));
    CHECK(macro::validAbbreviation("k2") && !macro::validAbbreviation("hi!") && !macro::validAbbreviation(std::string(40, 'a')));
    CHECK(expand("tks", false) == "cảm ơn");
    CHECK(expand("đc", false) == "được");
    CHECK(expand("tkss", false) == "<none>" && expand("Tks", false) == "<none>");
    // auto caps: first letter, all capitals, mixed = none
    CHECK(expand("Tks", true) == "Cảm ơn");
    CHECK(expand("TKS", true) == "CẢM ƠN");
    CHECK(expand("tKs", true) == "<none>");
    CHECK(expand("Đc", true) == "Được");
    CHECK(expand("ĐC", true) == "ĐƯỢC");
    CHECK(expand("tks", true) == "cảm ơn");
    // an exact entry wins over auto caps
    macro::set("Tks", "Thanks");
    CHECK(expand("Tks", true) == "Thanks" && expand("Tks", false) == "Thanks");
    // file format: UniKey/OpenKey lines, comment, BOM, CRLF, a bad line is skipped
    macro::parse("\xEF\xBB\xBF;Compatible OpenKey Macro Data file for UniKey*** version=1 ***\r\nms:millisecond\r\nbad line\r\n  btw : by the way \r\nx y:no\r\n");
    CHECK(macro::count() == 2 && expand("ms", false) == "millisecond" && expand("btw", false) == "by the way");
    // round trip through a file
    std::string path = std::string(getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp") + "/vikey_macro_test.txt";
    macro::clear(); macro::set("tks", "cảm ơn"); macro::set("vd", "ví dụ: a:b");
    CHECK(macro::save(path)); macro::clear(); CHECK(macro::count() == 0);
    CHECK(macro::load(path) && macro::count() == 2 && expand("vd", false) == "ví dụ: a:b" && expand("tks", false) == "cảm ơn");
    remove(path.c_str());
    CHECK(!macro::load("/nonexistent/dir/macros.txt") && macro::count() == 0);
    // OpenKey binary: count(2) , len, text, len(2), content
    const unsigned char bin[] = {1, 0, 3, 't', 'k', 's', 8, 0, 'c', 'a', 'm', ' ', 'o', 'n', '!', '?'};
    macro::loadOpenKeyBinary(bin, sizeof bin);
    CHECK(macro::count() == 1 && expand("tks", false) == "cam on!?");
    macro::loadOpenKeyBinary(bin, 5); CHECK(macro::count() == 0);   // truncated: nothing, no crash
    // triggers
    CHECK(macro::isTrigger(KEY_SPACE, false) && macro::isTrigger(KEY_RETURN, true) && macro::isTrigger(KEY_TAB, false) && macro::isTrigger(KEY_COMMA, false));
    CHECK(!macro::isTrigger(KEY_COMMA, true) && !macro::isTrigger(KEY_A, false) && !macro::isTrigger(KEY_SLASH, false));
    // English word tracker
    macro::englishReset();
    macro::englishType(KEY_T, false); macro::englishType(KEY_K, false); macro::englishType(KEY_S, true);
    CHECK(macro::toUtf8(macro::englishWord()) == "tkS");
    macro::englishBackspace(); CHECK(macro::toUtf8(macro::englishWord()) == "tk");
    macro::englishType(KEY_2, false); CHECK(macro::toUtf8(macro::englishWord()) == "tk2");
    macro::englishType(KEY_MINUS, false); CHECK(macro::englishWord().empty());   // another key ends it
    // the word on screen, read from the Vietnamese engine
    CHECK(screenWord("tks") == "tks");
    CHECK(screenWord("tieengs") == "tiếng");
    CHECK(screenWord("ddc") == "đc");
    CHECK(screenWord("vieetj") == "việt");
    CHECK(screenWord("") == "");
    CHECK(screenWord("abc ") == "");   // a space already follows: nothing to expand
    printf(fails ? "macro_test: %d FAILED\n" : "macro_test: ok\n", fails);
    return fails ? 1 : 0;
}
