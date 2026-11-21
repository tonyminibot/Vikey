// Live test for Vikey on Windows. Opens a window with an edit control, brings it to the front,
// types each case's keys with SendInput (as a user would; Vikey's hooks see them), and compares
// the text that ends up in the control with the expected text. Cases: cases.tsv (name<TAB>start
// language vi|en<TAB>keys<TAB>expected). Keys: letters, digits, space, punctuation as typed;
// \b Backspace, \n Enter, \t Tab, {alt+z} hotkey (default), {win+space} system hotkey override, {home} {end} {num0}..{num9} {add} {caps} named keys,
// {down:alt} {up:alt} (alt, ctrl, shift or one character) a key held across others or auto-repeating. Exit code = number of failures.
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>
#include "Prefs.h"
#include "../App/KeyMap.h"

static HWND _edit;
static WNDPROC _editProc = nullptr;
static int _altDDelivered = 0;
static LRESULT CALLBACK testEditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SYSKEYDOWN && w == 'D') ++_altDDelivered;
    return CallWindowProcW(_editProc, h, m, w, l);
}
struct SavedPref { bool exists = false; DWORD value = 0; };
static SavedPref savedDword(const wchar_t* name) {
    SavedPref p; HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Vikey", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD size = sizeof(p.value), type = 0; p.exists = RegQueryValueExW(key, name, nullptr, &type, (LPBYTE)&p.value, &size) == ERROR_SUCCESS && type == REG_DWORD; RegCloseKey(key);
    }
    return p;
}
static void setDword(const wchar_t* name, const SavedPref& value) {
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Vikey", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    if (value.exists) RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE*)&value.value, sizeof(value.value));
    else RegDeleteValueW(key, name);
    RegCloseKey(key);
}
static LRESULT CALLBACK testWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_SETFOCUS && _edit) { SetFocus(_edit); return 0; }
    return DefWindowProcW(h, m, w, l);
}
static void key(WORD vk, bool shift = false) {
    INPUT in[4] = {}; int n = 0;
    if (shift) { in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = VK_SHIFT; n++; }
    in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = vk; n++;
    in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = vk; in[n].ki.dwFlags = KEYEVENTF_KEYUP; n++;
    if (shift) { in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = VK_SHIFT; in[n].ki.dwFlags = KEYEVENTF_KEYUP; n++; }
    SendInput(n, in, sizeof(INPUT));
    for (int t = 0; t < 3; t++) { MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); } Sleep(5); }
}
static void clickEmojiWithMouse() {
    HWND picker = FindWindowW(L"VikeyEmoji", L"Vikey Emoji");
    if (!picker || !IsWindowVisible(picker)) return;
    DPI_AWARENESS_CONTEXT old = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    UINT dpi = GetDpiForWindow(picker);
    POINT p = {MulDiv(32, dpi, 96), MulDiv(107, dpi, 96)}, previous;
    GetCursorPos(&previous); ClientToScreen(picker, &p);
    int left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    INPUT events[3] = {};
    for (auto& e : events) e.type = INPUT_MOUSE;
    events[0].mi.dx = MulDiv(p.x - left, 65535, width - 1); events[0].mi.dy = MulDiv(p.y - top, 65535, height - 1);
    events[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    events[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN; events[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(3, events, sizeof(INPUT));
    for (int i = 0; i < 30; ++i) { MSG msg; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); } Sleep(10); }
    SetCursorPos(previous.x, previous.y);
    if (old) SetThreadDpiAwarenessContext(old);
}
static void typeKeys(const std::wstring& s) {
    for (size_t i = 0; i < s.size(); i++) {
        wchar_t c = s[i];
        if (c == L'\\' && i + 1 < s.size()) { wchar_t d = s[++i]; if (d == L'b') key(VK_BACK); else if (d == L'n') key(VK_RETURN); else if (d == L't') key(VK_TAB); continue; }
        if (c == L'{') { size_t e = s.find(L'}', i); std::wstring cmd = s.substr(i + 1, e - i - 1); i = e;
            if (cmd == L"alt+z") { INPUT in[4] = {}; in[0].type = in[1].type = in[2].type = in[3].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_MENU; in[1].ki.wVk = 'Z'; in[2].ki.wVk = 'Z'; in[2].ki.dwFlags = KEYEVENTF_KEYUP; in[3].ki.wVk = VK_MENU; in[3].ki.dwFlags = KEYEVENTF_KEYUP; SendInput(4, in, sizeof(INPUT)); Sleep(30); }
            else if (cmd == L"win+space") { INPUT in[4] = {}; in[0].type = in[1].type = in[2].type = in[3].type = INPUT_KEYBOARD; in[0].ki.wVk = VK_LWIN; in[1].ki.wVk = VK_SPACE; in[2].ki.wVk = VK_SPACE; in[2].ki.dwFlags = KEYEVENTF_KEYUP; in[3].ki.wVk = VK_LWIN; in[3].ki.dwFlags = KEYEVENTF_KEYUP; SendInput(4, in, sizeof(INPUT)); Sleep(30); }
            else if (cmd == L"emoji-alt-d") { INPUT in[4] = {}; for (auto& e : in) e.type = INPUT_KEYBOARD; in[0].ki.wVk = VK_MENU; in[1].ki.wVk = 'D'; in[2].ki.wVk = 'D'; in[2].ki.dwFlags = KEYEVENTF_KEYUP; in[3].ki.wVk = VK_MENU; in[3].ki.dwFlags = KEYEVENTF_KEYUP; SendInput(4, in, sizeof(INPUT)); Sleep(100); }
            else if (cmd == L"emoji") { INPUT in[6] = {}; for (auto& e : in) e.type = INPUT_KEYBOARD; in[0].ki.wVk = VK_CONTROL; in[1].ki.wVk = VK_MENU; in[2].ki.wVk = 'E'; in[3].ki.wVk = 'E'; in[3].ki.dwFlags = KEYEVENTF_KEYUP; in[4].ki.wVk = VK_MENU; in[4].ki.dwFlags = KEYEVENTF_KEYUP; in[5].ki.wVk = VK_CONTROL; in[5].ki.dwFlags = KEYEVENTF_KEYUP; SendInput(6, in, sizeof(INPUT)); Sleep(900); }
            else if (cmd == L"emoji-search-rocket") { HWND picker = FindWindowW(L"VikeyEmoji", L"Vikey Emoji"); if (picker) for (wchar_t ch : std::wstring(L"rocket")) SendMessageW(picker, WM_APP + 12, ch, 0); }
            else if (cmd == L"emoji-enter") { HWND picker = FindWindowW(L"VikeyEmoji", L"Vikey Emoji"); if (picker) SendMessageW(picker, WM_APP + 10, VK_RETURN, 0); Sleep(100); }
            else if (cmd == L"emoji-click-rocket") { HWND picker = FindWindowW(L"VikeyEmoji", L"Vikey Emoji"); if (picker) SendMessageW(picker, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(32, 107)); Sleep(100); }
            else if (cmd == L"emoji-physical-click") clickEmojiWithMouse();
            else if (cmd == L"emoji-select-rocket") { HWND picker = FindWindowW(L"VikeyEmoji", L"Vikey Emoji"); if (picker) SendMessageW(picker, WM_APP + 10, VK_RETURN, 0); Sleep(100); }
            else if (cmd.compare(0, 5, L"down:") == 0 || cmd.compare(0, 3, L"up:") == 0) {
                bool up = cmd[0] == L'u'; std::wstring n = cmd.substr(up ? 3 : 5);
                INPUT in = {}; in.type = INPUT_KEYBOARD; in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
                in.ki.wVk = n == L"alt" ? VK_MENU : n == L"ctrl" ? VK_CONTROL : n == L"shift" ? VK_SHIFT : LOBYTE(VkKeyScanW(n[0]));
                SendInput(1, &in, sizeof(INPUT)); Sleep(30); }
            else if (cmd == L"wait") Sleep(1500);
            else if (cmd == L"home") key(VK_HOME);
            else if (cmd == L"end") key(VK_END);
            else if (cmd == L"add") key(VK_ADD);
            else if (cmd == L"caps") key(VK_CAPITAL);   // toggles Caps Lock: a case turns it off again
            else if (cmd.size() == 4 && cmd.compare(0, 3, L"num") == 0 && cmd[3] >= L'0' && cmd[3] <= L'9') key((WORD)(VK_NUMPAD0 + (cmd[3] - L'0')));
            continue; }
        SHORT v = VkKeyScanW(c);
        if (v == -1) {   // no key for it on this layout (ô, í): type the character itself, as another input tool would
            INPUT in[2] = {}; in[0].type = in[1].type = INPUT_KEYBOARD; in[0].ki.wScan = in[1].ki.wScan = c; in[0].ki.dwFlags = KEYEVENTF_UNICODE; in[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
            SendInput(2, in, sizeof(INPUT)); Sleep(30); continue;
        }
        key(LOBYTE(v), (HIBYTE(v) & 1) != 0);
    }
}
static std::wstring editText() { int n = GetWindowTextLengthW(_edit); std::wstring s(n, 0); GetWindowTextW(_edit, &s[0], n + 1); return s; }
static std::wstring fromUtf8(const std::string& s) { int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0); std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); return w; }
static std::string toUtf8(const std::wstring& w) { int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr); std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr); return s; }

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::wstring casesPath = argc > 1 ? argv[1] : L"cases.tsv";
    std::ifstream f(casesPath); if (!f) { printf("cannot open cases file\n"); return 99; }
    HINSTANCE inst = GetModuleHandleW(nullptr);
    bool packetTarget = argc > 2 && wcscmp(argv[2], L"--packet-target") == 0;
    const wchar_t* editClass = L"EDIT";
    if (packetTarget) {
        // A native superclass with a different class name forces Vikey's SendInput path,
        // rather than its direct WM_CHAR special case for Edit/RichEdit controls.
        WNDCLASSW inputClass = {};
        if (!GetClassInfoW(nullptr, L"EDIT", &inputClass)) return 97;
        inputClass.hInstance = inst; inputClass.lpszClassName = L"VikeyPacketInput";
        if (!RegisterClassW(&inputClass)) return 97;
        editClass = L"VikeyPacketInput";
    }
    WNDCLASSW wc = {}; wc.lpfnWndProc = testWndProc; wc.hInstance = inst; wc.lpszClassName = L"VikeyLiveTest"; wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); RegisterClassW(&wc);
    HWND win = CreateWindowExW(WS_EX_TOPMOST, L"VikeyLiveTest", L"Vikey live test", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 600, 300, nullptr, nullptr, inst, nullptr);
    _edit = CreateWindowExW(0, editClass, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL, 10, 10, 560, 240, win, nullptr, inst, nullptr);
    if (!_edit) return 97;
    _editProc = (WNDPROC)SetWindowLongPtrW(_edit, GWLP_WNDPROC, (LONG_PTR)testEditProc);
    if (packetTarget) printf("input path: custom class, SendInput required\n");
    SendMessageW(_edit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    // Bring our window to the front even when started from a console (foreground lock): the Alt-key trick.
    auto toFront = [&]() {
        // attach to the foreground thread's input queue so SetForegroundWindow is allowed (no Alt key: it would open menu mode)
        DWORD fg = GetWindowThreadProcessId(GetForegroundWindow(), nullptr), me = GetCurrentThreadId();
        if (fg != me) AttachThreadInput(fg, me, TRUE);
        SetForegroundWindow(win); BringWindowToTop(win); SetActiveWindow(win); SetFocus(_edit);
        if (fg != me) AttachThreadInput(fg, me, FALSE);
        MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        SetForegroundWindow(win); SetActiveWindow(win); SetFocus(_edit);
    };
    for (int i = 0; i < 10; i++) { toFront(); if (GetForegroundWindow() == win && GetFocus() == _edit) break; Sleep(100); }
    printf("foreground is %s, focus is %s\n", GetForegroundWindow() == win ? "the test window" : "ANOTHER window", GetFocus() == _edit ? "the edit" : "elsewhere");
    Sleep(300);
    HWND tray = FindWindowW(L"VikeyTray", nullptr);
    if (!tray) { printf("Vikey is not running\n"); return 98; }
    SavedPref savedSwitch = savedDword(L"SwitchKeyStatus"), savedEmoji = savedDword(L"EmojiPicker"), savedEmojiHotkey = savedDword(L"EmojiHotkey");
    auto reloadPreferences = [&]() { SendMessageTimeoutW(tray, WM_APP + 8, 0, 0, SMTO_NORMAL, 2000, nullptr); };
    int fails = 0, total = 0; std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> col; std::stringstream ss(line); std::string c; while (std::getline(ss, c, '\t')) col.push_back(c);
        if (col.size() < 4) continue;
        total++;
        // set the start language through the registry + a hotkey-free path: the tray app re-reads InputMethod on reloadSettings;
        // the simplest robust way is to toggle with the hotkey until the tray state matches, so we read it back from the registry.
        int want = col[1] == "vi" ? 1 : 0;
        // Feature-specific regression cases install their shortcut temporarily, then the
        // previous per-user settings are restored after the suite finishes.
        if (col[0] == "winspace_switch") setDword(L"SwitchKeyStatus", {true, (DWORD)(KEY_SPACE | 0x400)});
        else setDword(L"SwitchKeyStatus", savedSwitch);
        if (col[0].find("emoji_") == 0) {
            setDword(L"EmojiPicker", {true, 1}); setDword(L"EmojiHotkey", {true, (DWORD)(col[0] == "emoji_alt_d_swallowed" ? (2 | 0x200) : (14 | 0x100 | 0x200))}); reloadPreferences();
        } else { setDword(L"EmojiPicker", savedEmoji); setDword(L"EmojiHotkey", savedEmojiHotkey); reloadPreferences(); }
        toFront();
        SetWindowTextW(_edit, L""); SetFocus(_edit); SendMessageW(_edit, EM_SETSEL, 0, 0); Sleep(150);
        key(VK_ESCAPE);   // Esc: a word break, ends whatever the previous case left
        Sleep(50);
        // set the start language without the hotkey (the hotkey pauses auto switch for 5 s); it also starts a
        // fresh session, so the first word of the case counts as a whole word (Esc alone would not)
        SendMessageTimeoutW(tray, WM_APP + 2, want, 0, SMTO_NORMAL, 2000, nullptr);
        Sleep(100);
        typeKeys(fromUtf8(col[2]));
        for (int t = 0; t < 20; t++) { MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); } Sleep(10); }
        std::wstring got = editText(); std::wstring exp = fromUtf8(col[3]);
        // the edit control uses \r\n
        std::wstring g2; for (wchar_t ch : got) if (ch != L'\r') g2 += ch;
        bool ok = col[0] == "emoji_choose_enter_inserts" ? !g2.empty() : g2 == exp;
        if (col[0] == "emoji_alt_d_swallowed") {
            HWND picker = FindWindowW(L"VikeyEmoji", L"Vikey Emoji");
            ok = picker && IsWindowVisible(picker) && GetForegroundWindow() == win && _altDDelivered == 0;
            printf("emoji Alt+D: picker=%d test-kept-focus=%d Alt+D-forwarded=%d\n", picker && IsWindowVisible(picker), GetForegroundWindow() == win, _altDDelivered != 0);
            if (picker) key(VK_ESCAPE);
        }
        if (!ok) fails++;
        printf("%s %s: got \"%s\" expected \"%s\"\n", ok ? "ok  " : "FAIL", col[0].c_str(), toUtf8(g2).c_str(), toUtf8(exp).c_str());
        key(VK_ESCAPE);
    }
    setDword(L"SwitchKeyStatus", savedSwitch); setDword(L"EmojiPicker", savedEmoji); setDword(L"EmojiHotkey", savedEmojiHotkey); reloadPreferences();
    printf("%d/%d passed\n", total - fails, total);
    DestroyWindow(win);
    return fails;
}
