//
//  Macro.cpp
//  Vikey
//
//  Gõ tắt (D68). See Macro.h.
//

#include "Macro.h"
#include "Vietnamese.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <unistd.h>

namespace vikey { namespace macro {

static std::map<std::string, std::string> _table;   // abbreviation (UTF-8) -> expansion (UTF-8)

// ---- UTF-8 -------------------------------------------------------------------------------------------
std::u32string fromUtf8(const std::string& s) {
    std::u32string out;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (n == 0 || i + n > s.size()) { i++; continue; }   // a broken byte is dropped
        char32_t cp = n == 1 ? c : (c & (0xFF >> (n + 1)));
        bool ok = true;
        for (int k = 1; k < n; k++) { unsigned char d = (unsigned char)s[i + k]; if ((d & 0xC0) != 0x80) { ok = false; break; } cp = (cp << 6) | (d & 0x3F); }
        if (ok) out += cp;
        i += ok ? n : 1;
    }
    return out;
}
std::string toUtf8(const std::u32string& s) {
    std::string out;
    for (char32_t c : s) {
        if (c < 0x80) out += (char)c;
        else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
        else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
    }
    return out;
}

// ---- case (Latin letters Vietnamese uses) -------------------------------------------------------------
static bool isUpperCp(char32_t c) {
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return true;
    if (c == 0x102 || c == 0x110 || c == 0x128 || c == 0x168 || c == 0x1A0 || c == 0x1AF) return true;
    return c >= 0x1EA0 && c <= 0x1EF8 && c % 2 == 0;
}
static char32_t lowerCp(char32_t c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (isUpperCp(c)) return c >= 0xC0 && c <= 0xDE ? c + 32 : c + 1;
    return c;
}
static bool isLowerCp(char32_t c) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return true;
    if (c == 0x103 || c == 0x111 || c == 0x129 || c == 0x169 || c == 0x1A1 || c == 0x1B0) return true;
    return c >= 0x1EA1 && c <= 0x1EF9 && c % 2 == 1;
}
static char32_t upperCp(char32_t c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    if (isLowerCp(c)) return c >= 0xE0 && c <= 0xFE ? c - 32 : c - 1;
    return c;
}
static bool isLetterCp(char32_t c) { return isUpperCp(c) || isLowerCp(c); }

// ---- the table ---------------------------------------------------------------------------------------
bool validAbbreviation(const std::string& a) {
    std::u32string u = fromUtf8(a);
    if (u.empty() || u.size() > 32) return false;
    for (char32_t c : u) if (!(isLetterCp(c) || (c >= '0' && c <= '9'))) return false;
    return true;
}
static std::string cleanExpansion(const std::string& e) {
    std::string o; for (char c : e) if (c != '\n' && c != '\r') o += c;
    size_t a = 0, b = o.size(); while (a < b && (o[a] == ' ' || o[a] == '\t')) a++; while (b > a && (o[b - 1] == ' ' || o[b - 1] == '\t')) b--;
    return o.substr(a, b - a);
}
bool set(const std::string& a, const std::string& e) {
    std::string x = cleanExpansion(e);
    if (!validAbbreviation(a) || x.empty()) return false;
    _table[a] = x; return true;
}
bool remove(const std::string& a) { return _table.erase(a) > 0; }
bool has(const std::string& a) { return _table.count(a) != 0; }
void clear() { _table.clear(); }
size_t count() { return _table.size(); }
std::vector<std::pair<std::string, std::string>> all() { return std::vector<std::pair<std::string, std::string>>(_table.begin(), _table.end()); }

void parse(const std::string& text) {
    _table.clear();
    std::istringstream in(text); std::string line; bool first = true;
    while (std::getline(in, line)) {
        if (first && line.size() >= 3 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) line = line.substr(3);   // BOM
        first = false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == ';') continue;
        size_t colon = line.find(':'); if (colon == std::string::npos) continue;
        std::string a = line.substr(0, colon); while (!a.empty() && (a.back() == ' ' || a.back() == '\t')) a.pop_back();
        while (!a.empty() && (a[0] == ' ' || a[0] == '\t')) a.erase(0, 1);
        set(a, line.substr(colon + 1));   // an invalid line is skipped
    }
}
std::string serialize() {
    std::string o = ";Vikey macro table (UniKey/OpenKey compatible): abbreviation:expansion\n";
    for (auto& kv : _table) o += kv.first + ":" + kv.second + "\n";
    return o;
}
bool load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { _table.clear(); return false; }
    std::stringstream ss; ss << f.rdbuf(); parse(ss.str()); return true;
}
bool save(const std::string& path) {
    std::string tmp = path + ".tmp" + std::to_string(getpid());   // the app and the keyboard process may both write
    { std::ofstream f(tmp, std::ios::binary); f << serialize(); if (!f) { ::remove(tmp.c_str()); return false; } }
    if (rename(tmp.c_str(), path.c_str()) == 0) return true;
    ::remove(tmp.c_str()); return false;
}
void loadOpenKeyBinary(const unsigned char* d, size_t size) {
    _table.clear();
    if (size < 2) return;
    size_t cur = 0; unsigned count = d[0] | (d[1] << 8); cur = 2;
    for (unsigned i = 0; i < count; i++) {
        if (cur >= size) return;
        size_t tl = d[cur++]; if (cur + tl + 2 > size) return;
        std::string text((const char*)d + cur, tl); cur += tl;
        size_t cl = d[cur] | (d[cur + 1] << 8); cur += 2; if (cur + cl > size) return;
        std::string content((const char*)d + cur, cl); cur += cl;
        set(text, content);
    }
}

bool lookup(const std::u32string& word, bool autoCaps, std::u32string& out) {
    if (_table.empty() || word.empty()) return false;
    auto it = _table.find(toUtf8(word));
    if (it != _table.end()) { out = fromUtf8(it->second); return true; }
    if (!autoCaps) return false;
    std::u32string low; bool upperFirst = isUpperCp(word[0]); bool anyUpperAfter = false, anyLowerAfter = false;
    for (size_t i = 0; i < word.size(); i++) {
        low += lowerCp(word[i]);
        if (i > 0 && isLetterCp(word[i])) { if (isUpperCp(word[i])) anyUpperAfter = true; else anyLowerAfter = true; }
    }
    if (!upperFirst) return false;   // lowercase first letter: no auto caps
    it = _table.find(toUtf8(low));
    if (it == _table.end()) return false;
    out = fromUtf8(it->second);
    if (anyUpperAfter && !anyLowerAfter && word.size() > 1) { for (auto& c : out) c = upperCp(c); }   // "TKS": all capitals
    else if (!anyUpperAfter) { if (!out.empty()) out[0] = upperCp(out[0]); }                          // "Tks": first letter
    else return false;   // "tKs": neither
    return true;
}

bool isTrigger(Uint16 key, bool shift) {
    if (key == KEY_SPACE || key == KEY_RETURN || key == KEY_ENTER || key == KEY_TAB) return true;
    return !shift && (key == KEY_COMMA || key == KEY_DOT || key == KEY_SEMICOLON);
}

// ---- English word ------------------------------------------------------------------------------------
static std::u32string _en;
void englishType(Uint16 key, bool caps) {
    bool letter = key == KEY_A || key == KEY_B || key == KEY_C || key == KEY_D || key == KEY_E || key == KEY_F || key == KEY_G || key == KEY_H || key == KEY_I ||
                  key == KEY_J || key == KEY_K || key == KEY_L || key == KEY_M || key == KEY_N || key == KEY_O || key == KEY_P || key == KEY_Q || key == KEY_R ||
                  key == KEY_S || key == KEY_T || key == KEY_U || key == KEY_V || key == KEY_W || key == KEY_X || key == KEY_Y || key == KEY_Z;
    if (!letter && !(IS_NUMBER_KEY(key) && !caps)) { _en.clear(); return; }
    Uint16 ch = keyCodeToCharacter(key | (caps && letter ? CAPS_MASK : 0));
    if (ch == 0 || _en.size() >= 32) { _en.clear(); return; }   // longer than any abbreviation: no word to match
    _en += (char32_t)ch;
}
void englishBackspace() { if (!_en.empty()) _en.pop_back(); }
void englishReset() { _en.clear(); }
const std::u32string& englishWord() { return _en; }

}}
