#include "TextExpansionStore.h"
#include "Prefs.h"

#include <windows.h>
#include <string>
#include <vector>

namespace vikey { namespace text_expansion_store {

static const LONGLONG MAX_TEXT_EXPANSION_FILE = 4LL * 1024 * 1024;

std::wstring path() { return prefs::appDataDir() + L"\\text-expansions.json"; }

TextExpansionIOResult load() {
    TextExpansionIOResult r;
    std::wstring p = path();
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND) r.ok = true;
        else r.error = "cannot open text-expansions.json";
        return r;
    }

    LARGE_INTEGER size = {};
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0 || size.QuadPart > MAX_TEXT_EXPANSION_FILE) {
        CloseHandle(h);
        r.error = "text-expansions.json is too large";
        return r;
    }
    if (size.QuadPart == 0) {
        CloseHandle(h);
        r.ok = true;
        return r;
    }

    std::string data((std::size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = ReadFile(h, &data[0], (DWORD)data.size(), &read, nullptr);
    CloseHandle(h);
    if (!ok || read != data.size()) {
        r.error = "cannot read text-expansions.json";
        return r;
    }
    return textExpansionParseJson(data);
}

bool save(const std::vector<TextExpansionEntry>& entries, std::string* error) {
    std::string data = textExpansionSerializeJson(entries);
    std::wstring dst = path();
    std::wstring tmp = dst + L".tmp";

    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        if (error) *error = "cannot create temporary text-expansions.json";
        return false;
    }

    DWORD written = 0;
    BOOL ok = data.empty() || WriteFile(h, data.data(), (DWORD)data.size(), &written, nullptr);
    if (ok) ok = FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok || written != data.size()) {
        DeleteFileW(tmp.c_str());
        if (error) *error = "cannot write text-expansions.json";
        return false;
    }

    if (!MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp.c_str());
        if (error) *error = "cannot replace text-expansions.json";
        return false;
    }
    return true;
}

}} // namespace
