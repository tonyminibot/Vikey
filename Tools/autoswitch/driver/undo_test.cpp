// Backspace-undo then typing on (D32): the word put back is the current word again.
// Also: a switch or a click starts fresh, Backspace doesn't reopen a word typed before it.
// Also: the word lookups the phone keyboards run between keys (suggestions, auto-correct) don't change what the
// next key does.
// usage: undo_test   (exit 1 on a failure)   keys: '<' Backspace, '|' VI/EN switch, '!' click (Mac, Windows, Linux),
//        capital letters with Shift, '?' before the keys: look the word up after every key like the phone keyboards
#include "Driver.h"
#include <cstdio>
#include <string>
int vLanguage = 1, vSwitchKeyStatus = 0, vCheckSpelling = 1, vUseModernOrthography = 1, vRestoreIfWrongSpelling = 1,
    vFixRecommendBrowser = 1, vUseSmartSwitchKey = 1, vOtherLanguage = 1, vAutoSwitchByTyping = 1;
static Uint16 K(char c) { static const Uint16 L[26] = {KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,KEY_Y,KEY_Z};
    return c == ' ' ? KEY_SPACE : c == '<' ? KEY_DELETE : L[(c | 0x20) - 'a']; }
static std::string type(const char* keys) {
    VikeyDriver d; vLanguage = 1; d.resetContext(); std::u16string t;
    bool lookups = *keys == '?'; if (lookups) keys++;
    std::string word;
    for (const char* p = keys; *p; p++) {
        if (*p == '|') { d.manualSwitch(0); continue; }
        if (*p == '!') { vKeyHandleEvent(vKeyEvent::Mouse, vKeyEventState::MouseDown, 0); continue; }   //what the desktop apps send on a click
        VKEdit e = d.press(K(*p), *p >= 'A' && *p <= 'Z', 0); for (int i = 0; i < e.deleteCount && !t.empty(); i++) t.pop_back(); t += e.text;
        if (*p == ' ') word.clear(); else if (*p == '<') { if (!word.empty()) word.pop_back(); } else word += (char)(*p | 0x20);
        if (lookups && !word.empty()) { vAutoSwitchWordScore(1, word.c_str(), nullptr, nullptr); vAutoSwitchIsVietnameseSpelling(word.c_str()); }
    }
    std::string o; for (char16_t c : t) { unsigned u = c; if (u < 0x80) o += (char)u; else if (u < 0x800) { o += (char)(0xC0 | u >> 6); o += (char)(0x80 | (u & 63)); } else { o += (char)(0xE0 | u >> 12); o += (char)(0x80 | ((u >> 6) & 63)); o += (char)(0x80 | (u & 63)); } }
    return o;
}
int main() {
    struct { const char* keys; const char* want; const char* why; } cases[] = {
        {"booj source vaay ", "bộ source vây ", "English word that is Vietnamese: fixed"},
        {"booj source vaay <", "bộ source vaay", "Backspace puts the keys back, without the space"},
        {"booj source vaay <j ", "bộ source vậy ", "typing on: the whole word is checked again (Tony 2026-10-29)"},
        {"booj source vaay < ", "bộ source vaay ", "the same word again stays as typed"},
        {"xin |hello|<s", "xin hells", "after a switch Backspace doesn't reopen xin (was xin heín)"},
        {"heelo!vieejt", "hêloviệt", "a click on a word put back to its keys: the next word gets Telex (was vieejt)"},
        {"?Terrible ", "Terrible ", "lookups between keys keep the capital of a doubled-key word (was terrible)"},
        {"?Sorry ", "Sorry ", "the same with the doubled key mid-word (was sorry)"},
        {"?tooi ddi hocj ", "tôi đi học ", "lookups don't change plain Vietnamese"},
    };
    int fails = 0;
    for (auto& c : cases) { std::string got = type(c.keys); if (got != c.want) { fails++; printf("FAIL: '%s' -> '%s', want '%s' (%s)\n", c.keys, got.c_str(), c.want, c.why); } }
    printf("undo after fix, switch, click: %s\n", fails ? "FAIL" : "ok");
    return fails ? 1 : 0;
}
