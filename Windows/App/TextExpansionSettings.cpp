#define _HAS_STD_BYTE 0
#include "TextExpansionSettings.h"
#include "TextExpansionStore.h"
#include "Prefs.h"
#include "Core.h"
#include "TextExpansionIO.h"

#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace vikey { namespace ui {
namespace {

enum {
    TX_ENABLE = 7001, TX_ENGLISH, TX_AUTOCAPS, TX_SEARCH, TX_LIST,
    TX_TRIGGER, TX_REPLACEMENT, TX_NEW, TX_SAVE, TX_DELETE, TX_IMPORT, TX_EXPORT, TX_STATUS
};

static HWND win = nullptr, ownerWin = nullptr, list = nullptr, search = nullptr, trigger = nullptr, replacement = nullptr;
static HWND enable = nullptr, english = nullptr, autoCaps = nullptr, del = nullptr, status = nullptr;
static HFONT font = nullptr, fontBold = nullptr;
static std::vector<TextExpansionEntry> entries;
static std::vector<std::size_t> visible;
static int selected = -1;

static std::wstring toW(const std::u16string& s) {
    std::wstring w; w.reserve(s.size());
    for (char16_t c : s) w.push_back((wchar_t)c);
    return w;
}
static std::u16string toU16(const std::wstring& w) {
    std::u16string s; s.reserve(w.size());
    for (wchar_t c : w) s.push_back((char16_t)c);
    return s;
}
static std::wstring textOf(HWND h) {
    int n = GetWindowTextLengthW(h);
    if (n <= 0) return L"";
    std::wstring s((std::size_t)n + 1, L'\0');
    GetWindowTextW(h, &s[0], n + 1);
    s.resize((std::size_t)n);
    return s;
}
static std::wstring lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], (DWORD)s.size());
    return s;
}
static bool containsInsensitive(const std::wstring& hay, const std::wstring& needle) {
    if (needle.empty()) return true;
    return lower(hay).find(lower(needle)) != std::wstring::npos;
}
static void setFont(HWND h, HFONT f = nullptr) { SendMessageW(h, WM_SETFONT, (WPARAM)(f ? f : font), TRUE); }
static HWND ctl(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, win, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    setFont(c); return c;
}
static void setStatus(const std::wstring& s) { if (status) SetWindowTextW(status, s.c_str()); }

static void refreshEnabled() {
    bool on = SendMessageW(enable, BM_GETCHECK, 0, 0) == BST_CHECKED;
    EnableWindow(english, on); EnableWindow(autoCaps, on);
}
static void loadEntries() {
    TextExpansionIOResult r = text_expansion_store::load();
    if (r.ok) {
        entries = std::move(r.entries);
        setStatus(std::to_wstring(entries.size()) + L" mục gõ tắt");
    } else {
        entries.clear();
        setStatus(L"Không đọc được text-expansions.json");
    }
}
static bool persist(const wchar_t* okMessage = nullptr) {
    std::string error;
    if (!text_expansion_store::save(entries, &error)) {
        MessageBoxW(win, L"Không thể lưu danh sách gõ tắt.", L"Vikey", MB_ICONERROR);
        return false;
    }
    core::reloadSettings();
    setStatus(okMessage ? okMessage : (std::to_wstring(entries.size()) + L" mục gõ tắt"));
    return true;
}
static void clearEditor() {
    selected = -1;
    SetWindowTextW(trigger, L""); SetWindowTextW(replacement, L"");
    ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    EnableWindow(del, FALSE);
    SetFocus(trigger);
}
static void rebuildList() {
    std::wstring q = textOf(search);
    visible.clear();
    ListView_DeleteAllItems(list);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        std::wstring a = toW(entries[i].trigger), b = toW(entries[i].replacement);
        if (!containsInsensitive(a, q) && !containsInsensitive(b, q)) continue;
        visible.push_back(i);
        LVITEMW item = {}; item.mask = LVIF_TEXT | LVIF_PARAM; item.iItem = (int)visible.size() - 1;
        item.pszText = const_cast<wchar_t*>(a.c_str()); item.lParam = (LPARAM)i;
        int row = ListView_InsertItem(list, &item);
        std::wstring one = b;
        std::replace(one.begin(), one.end(), L'\r', L' ');
        std::replace(one.begin(), one.end(), L'\n', L' ');
        ListView_SetItemText(list, row, 1, const_cast<wchar_t*>(one.c_str()));
    }
    setStatus(std::to_wstring(visible.size()) + (visible.size() == entries.size() ? L" mục gõ tắt" : L" kết quả / " + std::to_wstring(entries.size())));
}
static void selectRow(int row) {
    if (row < 0 || row >= (int)visible.size()) { clearEditor(); return; }
    selected = (int)visible[row];
    SetWindowTextW(trigger, toW(entries[selected].trigger).c_str());
    SetWindowTextW(replacement, toW(entries[selected].replacement).c_str());
    EnableWindow(del, TRUE);
}
static bool saveEditor() {
    std::wstring tw = textOf(trigger), rw = textOf(replacement);
    if (tw.empty()) { MessageBoxW(win, L"Từ gõ tắt không được để trống.", L"Vikey", MB_ICONWARNING); SetFocus(trigger); return false; }
    if (rw.empty()) { MessageBoxW(win, L"Nội dung thay thế không được để trống.", L"Vikey", MB_ICONWARNING); SetFocus(replacement); return false; }
    if (tw.find_first_of(L" \t\r\n") != std::wstring::npos) {
        MessageBoxW(win, L"Từ gõ tắt không được chứa khoảng trắng hoặc xuống dòng.", L"Vikey", MB_ICONWARNING); SetFocus(trigger); return false;
    }

    std::vector<TextExpansionEntry> before = entries;
    std::u16string t = toU16(tw), r = toU16(rw);
    int duplicate = -1;
    for (std::size_t i = 0; i < entries.size(); ++i)
        if ((int)i != selected && entries[i].trigger == t) { duplicate = (int)i; break; }
    if (duplicate >= 0) {
        if (MessageBoxW(win, L"Từ gõ tắt này đã tồn tại. Thay thế nội dung hiện có?", L"Vikey", MB_ICONQUESTION | MB_YESNO) != IDYES) return false;
        entries[duplicate].replacement = r;
        if (selected >= 0 && selected != duplicate) entries.erase(entries.begin() + selected);
    } else if (selected >= 0 && selected < (int)entries.size()) {
        entries[selected] = {t, r};
    } else entries.push_back({t, r});

    if (!persist(L"Đã lưu gõ tắt")) { entries = std::move(before); return false; }
    rebuildList(); clearEditor(); return true;
}
static void deleteSelected() {
    if (selected < 0 || selected >= (int)entries.size()) return;
    TextExpansionEntry removed = entries[selected];
    entries.erase(entries.begin() + selected);
    if (!persist(L"Đã xoá gõ tắt")) { entries.insert(entries.begin() + selected, removed); return; }
    rebuildList(); clearEditor();
}

static bool readUtf8File(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz = {}; if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 || sz.QuadPart > 4LL * 1024 * 1024) { CloseHandle(h); return false; }
    out.assign((std::size_t)sz.QuadPart, '\0'); DWORD got = 0;
    BOOL ok = out.empty() || ReadFile(h, &out[0], (DWORD)out.size(), &got, nullptr); CloseHandle(h);
    return ok && got == out.size();
}
static bool writeUtf8File(const std::wstring& path, const std::string& data) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wrote = 0; BOOL ok = data.empty() || WriteFile(h, data.data(), (DWORD)data.size(), &wrote, nullptr); CloseHandle(h);
    return ok && wrote == data.size();
}
static bool pickFile(bool save, std::wstring& path) {
    wchar_t buf[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {sizeof(ofn)}; ofn.hwndOwner = win; ofn.lpstrFile = buf; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"OpenKey / UniKey macro (*.txt)\0*.txt\0Tất cả tệp (*.*)\0*.*\0";
    ofn.lpstrDefExt = L"txt"; ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    if (!ok) return false; path = buf; return true;
}
static void importOpenKey() {
    std::wstring path; if (!pickFile(false, path)) return;
    std::string data; if (!readUtf8File(path, data)) { MessageBoxW(win, L"Không đọc được tệp.", L"Vikey", MB_ICONERROR); return; }
    TextExpansionIOResult r = textExpansionImportOpenKey(data);
    if (!r.ok) { MessageBoxW(win, L"Tệp macro không hợp lệ.", L"Vikey", MB_ICONERROR); return; }

    int replaced = 0, added = 0;
    for (const auto& e : r.entries) {
        auto it = std::find_if(entries.begin(), entries.end(), [&](const TextExpansionEntry& x) { return x.trigger == e.trigger; });
        if (it == entries.end()) { entries.push_back(e); ++added; }
        else { it->replacement = e.replacement; ++replaced; }
    }
    // Entries have already been merged above. If the atomic save fails, reload the last valid store.
    if (!persist()) { loadEntries(); rebuildList(); return; }
    rebuildList(); clearEditor();
    std::wstring msg = L"Đã nhập " + std::to_wstring(added) + L" mục mới";
    if (replaced) msg += L", cập nhật " + std::to_wstring(replaced) + L" mục";
    if (r.skipped) msg += L", bỏ qua " + std::to_wstring(r.skipped) + L" dòng không hợp lệ";
    MessageBoxW(win, msg.c_str(), L"Vikey", MB_ICONINFORMATION);
}
static void exportOpenKey() {
    std::wstring path; if (!pickFile(true, path)) return;
    TextExpansionExportResult r = textExpansionExportOpenKey(entries);
    if (!r.ok || !writeUtf8File(path, r.text)) { MessageBoxW(win, L"Không xuất được tệp.", L"Vikey", MB_ICONERROR); return; }
    std::wstring msg = L"Đã xuất " + std::to_wstring(r.exported) + L" mục.";
    if (r.skipped) msg += L" " + std::to_wstring(r.skipped) + L" mục không thể biểu diễn trong định dạng OpenKey/UniKey cũ đã được bỏ qua.";
    MessageBoxW(win, msg.c_str(), L"Vikey", MB_ICONINFORMATION);
}

static void setTogglePref(HWND c, const wchar_t* key) {
    prefs::setInt(key, SendMessageW(c, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0);
    core::reloadSettings(); refreshEnabled();
    if (ownerWin && IsWindow(ownerWin)) PostMessageW(ownerWin, WM_APP + 20, 0, 0);
}
static void layout(HWND h) {
    RECT rc; GetClientRect(h, &rc);
    int W = rc.right, pad = 18, y = 16;
    MoveWindow(enable, pad, y, 150, 24, TRUE); MoveWindow(english, pad + 165, y, 230, 24, TRUE); MoveWindow(autoCaps, pad + 410, y, 180, 24, TRUE);
    y += 38; MoveWindow(search, pad, y, W - 2 * pad, 28, TRUE);
    y += 38; int listH = 225; MoveWindow(list, pad, y, W - 2 * pad, listH, TRUE);
    y += listH + 12;
    HWND lab1 = GetDlgItem(h, 7101), lab2 = GetDlgItem(h, 7102);
    MoveWindow(lab1, pad, y + 4, 100, 22, TRUE); MoveWindow(trigger, pad + 105, y, W - pad - (pad + 105), 28, TRUE);
    y += 38; MoveWindow(lab2, pad, y + 4, 100, 22, TRUE); MoveWindow(replacement, pad + 105, y, W - pad - (pad + 105), 86, TRUE);
    y += 98;
    int bx = pad; for (int id : {TX_NEW, TX_SAVE, TX_DELETE, TX_IMPORT, TX_EXPORT}) {
        HWND b = GetDlgItem(h, id); int bw = id == TX_SAVE ? 105 : 88; MoveWindow(b, bx, y, bw, 30, TRUE); bx += bw + 8;
    }
    MoveWindow(status, bx + 8, y + 5, std::max(80, W - pad - bx - 8), 22, TRUE);
}
static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_SIZE: layout(h); return 0;
    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w); HWND c = (HWND)l;
        if (id == TX_ENABLE && code == BN_CLICKED) { setTogglePref(c, L"UseMacro"); return 0; }
        if (id == TX_ENGLISH && code == BN_CLICKED) { setTogglePref(c, L"UseMacroInEnglishMode"); return 0; }
        if (id == TX_AUTOCAPS && code == BN_CLICKED) { setTogglePref(c, L"vAutoCapsMacro"); return 0; }
        if (id == TX_SEARCH && code == EN_CHANGE) { rebuildList(); return 0; }
        if (id == TX_NEW && code == BN_CLICKED) { clearEditor(); return 0; }
        if (id == TX_SAVE && code == BN_CLICKED) { saveEditor(); return 0; }
        if (id == TX_DELETE && code == BN_CLICKED) { deleteSelected(); return 0; }
        if (id == TX_IMPORT && code == BN_CLICKED) { importOpenKey(); return 0; }
        if (id == TX_EXPORT && code == BN_CLICKED) { exportOpenKey(); return 0; }
        break; }
    case WM_NOTIFY: {
        NMHDR* n = (NMHDR*)l;
        if (n->idFrom == TX_LIST && n->code == LVN_ITEMCHANGED) {
            NMLISTVIEW* v = (NMLISTVIEW*)l;
            if ((v->uNewState & LVIS_SELECTED) && !(v->uOldState & LVIS_SELECTED)) selectRow(v->iItem);
            return 0;
        }
        if (n->idFrom == TX_LIST && n->code == NM_DBLCLK) {
            int row = ListView_GetNextItem(list, -1, LVNI_SELECTED); if (row >= 0) { selectRow(row); SetFocus(trigger); }
            return 0;
        }
        break; }
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: win = ownerWin = list = search = trigger = replacement = enable = english = autoCaps = del = status = nullptr; selected = -1; return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

} // namespace

void showTextExpansionSettings(HINSTANCE inst, HWND owner) {
    if (win) { ownerWin = owner; ShowWindow(win, SW_SHOW); SetForegroundWindow(win); return; }
    ownerWin = owner;
    INITCOMMONCONTROLSEX cc = {sizeof(cc), ICC_LISTVIEW_CLASSES}; InitCommonControlsEx(&cc);
    if (!font) {
        font = CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        fontBold = CreateFontW(-15,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
    }
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {}; wc.lpfnWndProc = proc; wc.hInstance = inst; wc.lpszClassName = L"VikeyTextExpansionSettings";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassW(&wc); registered = true;
    }
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN;
    RECT wr = {0,0,760,590}; AdjustWindowRectEx(&wr,style,FALSE,0);
    win = CreateWindowExW(0,L"VikeyTextExpansionSettings",L"Vikey – Gõ tắt",style,CW_USEDEFAULT,CW_USEDEFAULT,wr.right-wr.left,wr.bottom-wr.top,owner,nullptr,inst,nullptr);

    enable = ctl(L"BUTTON",L"Bật gõ tắt",BS_AUTOCHECKBOX|WS_TABSTOP,0,0,0,0,TX_ENABLE);
    english = ctl(L"BUTTON",L"Gõ tắt trong chế độ tiếng Anh",BS_AUTOCHECKBOX|WS_TABSTOP,0,0,0,0,TX_ENGLISH);
    autoCaps = ctl(L"BUTTON",L"Tự viết hoa",BS_AUTOCHECKBOX|WS_TABSTOP,0,0,0,0,TX_AUTOCAPS);
    SendMessageW(enable,BM_SETCHECK,prefs::getInt(L"UseMacro",1)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(english,BM_SETCHECK,prefs::getInt(L"UseMacroInEnglishMode",1)?BST_CHECKED:BST_UNCHECKED,0);
    SendMessageW(autoCaps,BM_SETCHECK,prefs::getInt(L"vAutoCapsMacro",0)?BST_CHECKED:BST_UNCHECKED,0);

    search = ctl(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,TX_SEARCH);
    SendMessageW(search,EM_SETCUEBANNER,TRUE,(LPARAM)L"Tìm theo từ gõ tắt hoặc nội dung…");

    list = ctl(WC_LISTVIEWW,L"",WS_BORDER|WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,0,0,0,0,TX_LIST);
    ListView_SetExtendedListViewStyle(list,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES);
    LVCOLUMNW col={}; col.mask=LVCF_TEXT|LVCF_WIDTH; col.cx=190; col.pszText=(LPWSTR)L"Từ gõ tắt"; ListView_InsertColumn(list,0,&col);
    col.cx=500; col.pszText=(LPWSTR)L"Nội dung"; ListView_InsertColumn(list,1,&col);

    HWND l1=ctl(L"STATIC",L"Từ gõ tắt",SS_LEFT,0,0,0,0,7101); setFont(l1,fontBold);
    trigger=ctl(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,0,0,0,0,TX_TRIGGER);
    SendMessageW(trigger,EM_SETLIMITTEXT,255,0);
    HWND l2=ctl(L"STATIC",L"Nội dung",SS_LEFT,0,0,0,0,7102); setFont(l2,fontBold);
    replacement=ctl(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,0,0,0,0,TX_REPLACEMENT);
    SendMessageW(replacement,EM_SETLIMITTEXT,16384,0);

    ctl(L"BUTTON",L"Mới",WS_TABSTOP,0,0,0,0,TX_NEW);
    ctl(L"BUTTON",L"Thêm / Lưu",WS_TABSTOP|BS_DEFPUSHBUTTON,0,0,0,0,TX_SAVE);
    del=ctl(L"BUTTON",L"Xoá",WS_TABSTOP,0,0,0,0,TX_DELETE); EnableWindow(del,FALSE);
    ctl(L"BUTTON",L"Nhập…",WS_TABSTOP,0,0,0,0,TX_IMPORT);
    ctl(L"BUTTON",L"Xuất…",WS_TABSTOP,0,0,0,0,TX_EXPORT);
    status=ctl(L"STATIC",L"",SS_RIGHT,0,0,0,0,TX_STATUS);

    loadEntries(); rebuildList(); refreshEnabled(); layout(win);

    RECT rc; GetWindowRect(win,&rc); int ww=rc.right-rc.left, hh=rc.bottom-rc.top;
    RECT work; SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    SetWindowPos(win,nullptr,(work.left+work.right-ww)/2,(work.top+work.bottom-hh)/2,0,0,SWP_NOSIZE|SWP_NOZORDER);
    ShowWindow(win,SW_SHOW); UpdateWindow(win); SetFocus(search);
}

}} // namespace vikey::ui
