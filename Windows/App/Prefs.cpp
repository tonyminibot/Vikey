#include "Prefs.h"
#include "TextExpansionIO.h"
#include <shlobj.h>
#include <fstream>
#include <unordered_map>

namespace vikey { namespace prefs {

static const wchar_t* ROOT = L"Software\\Vikey";

static HKEY open(REGSAM access) {
    HKEY h = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, ROOT, 0, nullptr, REG_OPTION_NON_VOLATILE, access, nullptr, &h, nullptr) != ERROR_SUCCESS) return nullptr;
    return h;
}

bool has(const wchar_t* key) {
    HKEY h = open(KEY_READ); if (!h) return false;
    DWORD type = 0, size = 0;
    bool ok = RegQueryValueExW(h, key, nullptr, &type, nullptr, &size) == ERROR_SUCCESS;
    RegCloseKey(h); return ok;
}

int getInt(const wchar_t* key, int fallback) {
    HKEY h = open(KEY_READ); if (!h) return fallback;
    DWORD v = 0, size = sizeof(v), type = 0;
    int r = fallback;
    if (RegQueryValueExW(h, key, nullptr, &type, (LPBYTE)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) r = (int)v;
    RegCloseKey(h); return r;
}

void setInt(const wchar_t* key, int value) {
    HKEY h = open(KEY_WRITE); if (!h) return;
    DWORD v = (DWORD)value;
    RegSetValueExW(h, key, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
    RegCloseKey(h);
}

std::vector<BYTE> getBinary(const wchar_t* key) {
    std::vector<BYTE> out;
    HKEY h = open(KEY_READ); if (!h) return out;
    DWORD size = 0, type = 0;
    if (RegQueryValueExW(h, key, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 0) {
        out.resize(size);
        if (RegQueryValueExW(h, key, nullptr, &type, out.data(), &size) != ERROR_SUCCESS) out.clear();
    }
    RegCloseKey(h); return out;
}

void setBinary(const wchar_t* key, const BYTE* data, DWORD size) {
    HKEY h = open(KEY_WRITE); if (!h) return;
    RegSetValueExW(h, key, 0, REG_BINARY, data, size);
    RegCloseKey(h);
}

std::wstring getString(const wchar_t* key, const std::wstring& fallback) {
    HKEY h = open(KEY_READ); if (!h) return fallback;
    DWORD size = 0, type = 0;
    std::wstring r = fallback;
    // size first, like getBinary: a fixed 1024-character buffer gave ERROR_MORE_DATA (so the fallback) for long values (EmojiUsage)
    if (RegQueryValueExW(h, key, nullptr, &type, nullptr, &size) == ERROR_SUCCESS && type == REG_SZ) {
        std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);   // +1: a terminator even if the stored value has none
        if (RegQueryValueExW(h, key, nullptr, &type, (LPBYTE)buf.data(), &size) == ERROR_SUCCESS && type == REG_SZ) r = buf.data();
    }
    RegCloseKey(h); return r;
}

void setString(const wchar_t* key, const std::wstring& value) {
    HKEY h = open(KEY_WRITE); if (!h) return;
    RegSetValueExW(h, key, 0, REG_SZ, (const BYTE*)value.c_str(), (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
}

void remove(const wchar_t* key) {
    HKEY h = open(KEY_WRITE); if (!h) return;
    RegDeleteValueW(h, key); RegCloseKey(h);
}

std::wstring appDataDir() {
    wchar_t path[MAX_PATH] = {0};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path);
    std::wstring dir = std::wstring(path) + L"\\Vikey";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

static const wchar_t* RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool launchAtLogin() {
    HKEY h; if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_READ, &h) != ERROR_SUCCESS) return false;
    DWORD size = 0; bool ok = RegQueryValueExW(h, L"Vikey", nullptr, nullptr, nullptr, &size) == ERROR_SUCCESS;
    RegCloseKey(h); return ok;
}

void setLaunchAtLogin(bool on) {
    HKEY h; if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_WRITE, &h) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(exe) + L"\"";
        RegSetValueExW(h, L"Vikey", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
    } else RegDeleteValueW(h, L"Vikey");
    RegCloseKey(h);
}

/// OpenKey's input type 2 / 3 were Simple Telex 1 / 2 (D56; VNI, 1, isn't in Vikey). Its own one-time step, so people
/// who moved from OpenKey before Vikey had the setting get it too; a style already chosen in Vikey stays.
static void telexStyleFromOpenKey() {
    if (has(L"MigratedTelexStyle")) return;
    if (has(L"TelexStyle")) { setInt(L"MigratedTelexStyle", 1); return; }
    // done only once OpenKey's setting was read: if it can't be read now, the next start tries again
    HKEY h; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\TuyenMai\\OpenKey", 0, KEY_READ, &h) != ERROR_SUCCESS) return;
    DWORD v = 0, size = sizeof(v), type = 0;
    if (RegQueryValueExW(h, L"vInputType", nullptr, &type, (LPBYTE)&v, &size) == ERROR_SUCCESS && type == REG_DWORD) {
        if (v == 2 || v == 3) setInt(L"TelexStyle", (int)v - 1);
        setInt(L"MigratedTelexStyle", 1);
    }
    RegCloseKey(h);
}

static bool saveTextExpansionsForMigration(const std::vector<TextExpansionEntry>& entries) {
    std::string json = textExpansionSerializeJson(entries);
    std::wstring dst = appDataDir() + L"\\text-expansions.json";
    std::wstring tmp = dst + L".migration.tmp";
    HANDLE fh = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (fh == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0; BOOL ok = json.empty() || WriteFile(fh, json.data(), (DWORD)json.size(), &written, nullptr);
    if (ok) ok = FlushFileBuffers(fh);
    CloseHandle(fh);
    if (!ok || written != json.size()) { DeleteFileW(tmp.c_str()); return false; }
    if (!MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(tmp.c_str()); return false; }
    return true;
}

static std::vector<TextExpansionEntry> loadExistingTextExpansionsForMigration() {
    std::vector<TextExpansionEntry> entries;
    std::wstring p = appDataDir() + L"\\text-expansions.json";
    HANDLE fh = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE) return entries;
    LARGE_INTEGER sz = {}; if (!GetFileSizeEx(fh, &sz) || sz.QuadPart <= 0 || sz.QuadPart > 4LL * 1024 * 1024) { CloseHandle(fh); return entries; }
    std::string data((std::size_t)sz.QuadPart, '\0'); DWORD read = 0;
    BOOL ok = ReadFile(fh, &data[0], (DWORD)data.size(), &read, nullptr); CloseHandle(fh);
    if (!ok || read != data.size()) return entries;
    TextExpansionIOResult r = textExpansionParseJson(data); if (r.ok) entries = r.entries;
    return entries;
}

static void migrateTextExpansionsFromOpenKey(HKEY h) {
    if (has(L"MigratedTextExpansionsFromOpenKey")) return;

    // Preserve the user's current Vikey settings. Only fill missing scalar preferences from OpenKey.
    struct { const wchar_t* from; const wchar_t* to; int fallback; } toggles[] = {
        {L"vUseMacro", L"UseMacro", 1},
        {L"vUseMacroInEnglishMode", L"UseMacroInEnglishMode", 0},
        {L"vAutoCapsMacro", L"vAutoCapsMacro", 0},
    };
    for (auto& m : toggles) {
        if (has(m.to)) continue;
        DWORD v = 0, size = sizeof(v), type = 0;
        if (RegQueryValueExW(h, m.from, nullptr, &type, (LPBYTE)&v, &size) == ERROR_SUCCESS && type == REG_DWORD)
            setInt(m.to, (int)v);
    }

    DWORD size = 0, type = 0;
    LONG q = RegQueryValueExW(h, L"macroData", nullptr, &type, nullptr, &size);
    if (q == ERROR_FILE_NOT_FOUND) { setInt(L"MigratedTextExpansionsFromOpenKey", 1); return; }
    if (q != ERROR_SUCCESS || type != REG_BINARY || size == 0 || size > 4u * 1024u * 1024u) return;

    std::vector<BYTE> data(size);
    if (RegQueryValueExW(h, L"macroData", nullptr, &type, data.data(), &size) != ERROR_SUCCESS) return;
    TextExpansionIOResult imported = textExpansionImportLegacyOpenKeyBinary(data.data(), data.size());
    if (!imported.ok) return;

    std::vector<TextExpansionEntry> merged = loadExistingTextExpansionsForMigration();
    std::unordered_map<std::u16string, std::size_t> existing;
    for (std::size_t i = 0; i < merged.size(); ++i) existing[merged[i].trigger] = i;
    for (const auto& e : imported.entries) {
        if (existing.find(e.trigger) != existing.end()) continue; // Vikey's current entry wins.
        existing[e.trigger] = merged.size();
        merged.push_back(e);
    }
    if (!saveTextExpansionsForMigration(merged)) return;
    setInt(L"MigratedTextExpansionsFromOpenKey", 1);
}

void migrateFromOpenKey() {
    telexStyleFromOpenKey();
    HKEY h; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\TuyenMai\\OpenKey", 0, KEY_READ, &h) != ERROR_SUCCESS) return;
    migrateTextExpansionsFromOpenKey(h);
    if (has(L"MigratedFromOpenKey")) { RegCloseKey(h); return; }
    setInt(L"MigratedFromOpenKey", 1);
    struct { const wchar_t* from; const wchar_t* to; } map[] = {
        {L"vLanguage", L"InputMethod"}, {L"vCheckSpelling", L"Spelling"}, {L"vRestoreIfWrongSpelling", L"RestoreIfInvalidWord"},
        {L"vUseModernOrthography", L"ModernOrthography"}, {L"vUseSmartSwitchKey", L"UseSmartSwitchKey"}, {L"vSwitchKeyStatus", L"SwitchKeyStatus"},
    };
    for (auto& m : map) {
        DWORD v = 0, size = sizeof(v), type = 0;
        if (RegQueryValueExW(h, m.from, nullptr, &type, (LPBYTE)&v, &size) == ERROR_SUCCESS && type == REG_DWORD && !has(m.to)) setInt(m.to, (int)v);
    }
    DWORD size = 0, type = 0;
    if (RegQueryValueExW(h, L"smartSwitchKey", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && size > 0 && !has(L"smartSwitchKey")) {
        std::vector<BYTE> d(size);
        if (RegQueryValueExW(h, L"smartSwitchKey", nullptr, &type, d.data(), &size) == ERROR_SUCCESS) setBinary(L"smartSwitchKey", d.data(), size);
    }
    RegCloseKey(h);
}

}} // namespace
