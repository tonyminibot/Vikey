// Backspace-undo then typing on (D32): the word put back is the current word again.
// Also: a switch or a click starts fresh, Backspace doesn't reopen a word typed before it.
// And: the keyboards' suggestion lookups between keys don't change what a word becomes.
// usage: undo_test   (exit 1 on a failure)   keys: '<' Backspace, '|' VI/EN switch, '!' click (Mac, Windows, Linux),
//        '~' from here a suggestion lookup of the word so far after every key (iOS, Android)
#include "Driver.h"
#include <cstdio>
#include <string>
int vLanguage = 1, vSwitchKeyStatus = 0, vCheckSpelling = 1, vUseModernOrthography = 1, vRestoreIfWrongSpelling = 1,
    vFixRecommendBrowser = 1, vUseSmartSwitchKey = 1, vOtherLanguage = 1, vAutoSwitchByTyping = 1;
static Uint16 K(char c) { static const Uint16 L[26] = {KEY_A,KEY_B,KEY_C,KEY_D,KEY_E,KEY_F,KEY_G,KEY_H,KEY_I,KEY_J,KEY_K,KEY_L,KEY_M,KEY_N,KEY_O,KEY_P,KEY_Q,KEY_R,KEY_S,KEY_T,KEY_U,KEY_V,KEY_W,KEY_X,KEY_Y,KEY_Z};
    return c == ' ' ? KEY_SPACE : c == '<' ? KEY_DELETE : L[c - 'a']; }
static std::string type(const char* keys) {
    VikeyDriver d; vLanguage = 1; d.resetContext(); std::u16string t; bool look = false; std::string word;
    for (const char* p = keys; *p; p++) {
        if (*p == '|') { d.manualSwitch(0); continue; }
        if (*p == '!') { vKeyHandleEvent(vKeyEvent::Mouse, vKeyEventState::MouseDown, 0); continue; }   //what the desktop apps send on a click
        if (*p == '~') { look = true; continue; }
        bool up = *p >= 'A' && *p <= 'Z'; char c = up ? *p + 32 : *p;
        VKEdit e = d.press(K(c), up, 0); for (int i = 0; i < e.deleteCount && !t.empty(); i++) t.pop_back(); t += e.text;
        if (c >= 'a' && c <= 'z') word += c; else word.clear();
        if (look && !word.empty()) vAutoSwitchWordScore(1, word.c_str(), nullptr, nullptr); }   //what VKCandidates / VKWordScore ask
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
        {"~Terrible ", "Terrible ", "suggestions on: the doubled key's word keeps its capital (was terrible)"},
    };
    int fails = 0;
    for (auto& c : cases) { std::string got = type(c.keys); if (got != c.want) { fails++; printf("FAIL: '%s' -> '%s', want '%s' (%s)\n", c.keys, got.c_str(), c.want, c.why); } }
    printf("undo after fix, switch, click, suggestions: %s\n", fails ? "FAIL" : "ok");
    return fails ? 1 : 0;
}
