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

}} // namespace
