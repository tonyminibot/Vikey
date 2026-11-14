//
//  TextExpansionIO.cpp
//  Vikey
//

#include "TextExpansionIO.h"

#include <cctype>
#include <cstring>
#include <limits>
#include <unordered_map>

namespace vikey {
namespace {

static void append16(std::u16string& out, std::uint32_t cp) {
    if (cp <= 0xFFFF) out.push_back((char16_t)cp);
    else {
        cp -= 0x10000;
        out.push_back((char16_t)(0xD800 + (cp >> 10)));
        out.push_back((char16_t)(0xDC00 + (cp & 0x3FF)));
    }
}

static bool utf8To16(const std::string& s, std::u16string& out) {
    out.clear();
    for (std::size_t i = 0; i < s.size();) {
        unsigned char a = (unsigned char)s[i++];
        std::uint32_t cp = 0, min = 0;
        int n = 0;
        if (a < 0x80) cp = a;
        else if ((a & 0xE0) == 0xC0) { cp = a & 0x1F; n = 1; min = 0x80; }
        else if ((a & 0xF0) == 0xE0) { cp = a & 0x0F; n = 2; min = 0x800; }
        else if ((a & 0xF8) == 0xF0) { cp = a & 0x07; n = 3; min = 0x10000; }
        else return false;
        if (i + (std::size_t)n > s.size()) return false;
        for (int k = 0; k < n; ++k) {
            unsigned char b = (unsigned char)s[i++];
            if ((b & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (b & 0x3F);
        }
        if (n && (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))) return false;
        append16(out, cp);
    }
    return true;
}

static void append8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7F) out.push_back((char)cp);
    else if (cp <= 0x7FF) {
        out.push_back((char)(0xC0 | (cp >> 6)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back((char)(0xE0 | (cp >> 12)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

static bool utf16To8(const std::u16string& s, std::string& out) {
    out.clear();
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::uint32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (++i >= s.size()) return false;
            std::uint32_t lo = s[i];
            if (lo < 0xDC00 || lo > 0xDFFF) return false;
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) return false;
        append8(out, cp);
    }
    return true;
}

static void hex4(std::string& out, std::uint16_t v) {
    static const char* H = "0123456789ABCDEF";
    out += "\\u";
    for (int sh : {12, 8, 4, 0}) out.push_back(H[(v >> sh) & 15]);
}

static std::string quote(const std::u16string& s) {
    std::string out = "\"";
    for (std::size_t i = 0; i < s.size(); ++i) {
        std::uint16_t c = s[i];
        switch (c) {
            case u'"': out += "\\\""; break;
            case u'\\': out += "\\\\"; break;
            case u'\b': out += "\\b"; break;
            case u'\f': out += "\\f"; break;
            case u'\n': out += "\\n"; break;
            case u'\r': out += "\\r"; break;
            case u'\t': out += "\\t"; break;
            default:
                if (c < 0x20) hex4(out, c);
                else if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
                    std::uint32_t cp = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
                    append8(out, cp);
                } else if (c >= 0xD800 && c <= 0xDFFF) hex4(out, c);
                else append8(out, c);
        }
    }
    out.push_back('"');
    return out;
}

static void add(TextExpansionIOResult& r, const TextExpansionEntry& e,
                std::unordered_map<std::u16string, std::size_t>& idx) {
    if (e.trigger.empty() || e.replacement.empty()) { ++r.skipped; return; }
    auto it = idx.find(e.trigger);
    if (it == idx.end()) {
        idx[e.trigger] = r.entries.size();
        r.entries.push_back(e);
        ++r.imported;
    } else {
        r.entries[it->second].replacement = e.replacement;
        ++r.replaced;
    }
}

class Json {
public:
    explicit Json(const std::string& x) : s(x) {}

    TextExpansionIOResult run() {
        TextExpansionIOResult r;
        std::unordered_map<std::u16string, std::size_t> idx;
        if (!take('{')) return bad("expected object");
        bool hv = false, he = false; int ver = -1;
        if (!see('}')) for (;;) {
            std::u16string k;
            if (!str(k) || !take(':')) return bad("invalid object member");
            if (k == u"version") { if (!integer(ver)) return bad("invalid version"); hv = true; }
            else if (k == u"entries") { if (!entries(r, idx)) return bad("invalid entries"); he = true; }
            else if (!skip()) return bad("invalid unknown field");
            if (take('}')) break;
            if (!take(',')) return bad("expected comma");
        } else take('}');
        ws();
        if (p != s.size()) return bad("trailing JSON data");
        if (!hv || ver != 1) return bad(hv ? "unsupported version" : "missing version");
        if (!he) return bad("missing entries");
        r.ok = true; return r;
    }

private:
    const std::string& s; std::size_t p = 0;

    TextExpansionIOResult bad(const char* e) { TextExpansionIOResult r; r.error = e; return r; }
    void ws() { while (p < s.size() && std::isspace((unsigned char)s[p])) ++p; }
    bool see(char c) { ws(); return p < s.size() && s[p] == c; }
    bool take(char c) { ws(); if (p < s.size() && s[p] == c) { ++p; return true; } return false; }

    static int hx(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    bool u4(std::uint16_t& v) {
        if (p + 4 > s.size()) return false; v = 0;
        for (int i = 0; i < 4; ++i) { int h = hx(s[p++]); if (h < 0) return false; v = (std::uint16_t)((v << 4) | h); }
        return true;
    }

    bool str(std::u16string& out) {
        ws(); if (p >= s.size() || s[p++] != '"') return false; out.clear();
        while (p < s.size()) {
            unsigned char c = (unsigned char)s[p++];
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c == '\\') {
                if (p >= s.size()) return false;
                switch (s[p++]) {
                    case '"': out.push_back(u'"'); break; case '\\': out.push_back(u'\\'); break; case '/': out.push_back(u'/'); break;
                    case 'b': out.push_back(u'\b'); break; case 'f': out.push_back(u'\f'); break; case 'n': out.push_back(u'\n'); break;
                    case 'r': out.push_back(u'\r'); break; case 't': out.push_back(u'\t'); break;
                    case 'u': {
                        std::uint16_t hi; if (!u4(hi)) return false;
                        if (hi >= 0xD800 && hi <= 0xDBFF) {
                            if (p + 2 > s.size() || s[p++] != '\\' || s[p++] != 'u') return false;
                            std::uint16_t lo; if (!u4(lo) || lo < 0xDC00 || lo > 0xDFFF) return false;
                            out.push_back((char16_t)hi); out.push_back((char16_t)lo);
                        } else if (hi >= 0xDC00 && hi <= 0xDFFF) return false;
                        else out.push_back((char16_t)hi);
                        break;
                    }
                    default: return false;
                }
            } else if (c < 0x80) out.push_back((char16_t)c);
            else {
                std::size_t q = p - 1, end = q + 1;
                int n = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
                if (n < 0 || end + n > s.size()) return false;
                end += n; std::u16string tmp;
                if (!utf8To16(s.substr(q, end - q), tmp)) return false;
                out += tmp; p = end;
            }
        }
        return false;
    }

    bool integer(int& v) {
        ws(); if (p >= s.size()) return false;
        bool neg = s[p] == '-'; if (neg) ++p;
        if (p >= s.size() || !std::isdigit((unsigned char)s[p])) return false;
        long long n = 0; while (p < s.size() && std::isdigit((unsigned char)s[p])) n = n * 10 + (s[p++] - '0');
        if (n > std::numeric_limits<int>::max() || (p < s.size() && (s[p] == '.' || s[p] == 'e' || s[p] == 'E'))) return false;
        v = (int)(neg ? -n : n); return true;
    }

    bool entries(TextExpansionIOResult& r, std::unordered_map<std::u16string, std::size_t>& idx) {
        if (!take('[')) return false;
        if (take(']')) return true;
        for (;;) {
            if (!see('{')) { if (!skip()) return false; ++r.skipped; }
            else {
                take('{'); TextExpansionEntry e; bool ht = false, hr = false, valid = true;
                if (!take('}')) for (;;) {
                    std::u16string k; if (!str(k) || !take(':')) return false;
                    if (k == u"trigger") { if (see('"')) { if (!str(e.trigger)) return false; ht = true; } else { if (!skip()) return false; valid = false; } }
                    else if (k == u"replacement") { if (see('"')) { if (!str(e.replacement)) return false; hr = true; } else { if (!skip()) return false; valid = false; } }
                    else if (!skip()) return false;
                    if (take('}')) break; if (!take(',')) return false;
                }
                if (valid && ht && hr) add(r, e, idx); else ++r.skipped;
            }
            if (take(']')) return true; if (!take(',')) return false;
        }
    }

    bool skip(int d = 0) {
        if (d > 64) return false; ws(); if (p >= s.size()) return false;
        if (see('"')) { std::u16string x; return str(x); }
        if (take('{')) {
            if (take('}')) return true;
            for (;;) { std::u16string k; if (!str(k) || !take(':') || !skip(d + 1)) return false; if (take('}')) return true; if (!take(',')) return false; }
        }
        if (take('[')) {
            if (take(']')) return true;
            for (;;) { if (!skip(d + 1)) return false; if (take(']')) return true; if (!take(',')) return false; }
        }
        for (const char* x : {"true","false","null"}) { std::size_t n = std::strlen(x); if (s.compare(p,n,x)==0) { p += n; return true; } }
        std::size_t b = p; if (p < s.size() && s[p] == '-') ++p;
        if (p >= s.size() || !std::isdigit((unsigned char)s[p])) { p = b; return false; }
        while (p < s.size() && (std::isdigit((unsigned char)s[p]) || s[p]=='.' || s[p]=='e' || s[p]=='E' || s[p]=='+' || s[p]=='-')) ++p;
        return p > b;
    }
};

static bool lineBreak(const std::u16string& s) {
    return s.find(u'\n') != std::u16string::npos || s.find(u'\r') != std::u16string::npos;
}

} // namespace

std::string textExpansionSerializeJson(const std::vector<TextExpansionEntry>& entries) {
    std::string out = "{\n  \"version\": 1,\n  \"entries\": [";
    bool first = true;
    for (const auto& e : entries) {
        if (e.trigger.empty() || e.replacement.empty()) continue;
        out += first ? "\n" : ",\n"; first = false;
        out += "    {\"trigger\": " + quote(e.trigger) + ", \"replacement\": " + quote(e.replacement) + "}";
    }
    if (!first) out += "\n  ";
    return out + "]\n}\n";
}

TextExpansionIOResult textExpansionParseJson(const std::string& json) {
    std::string s = json;
    if (s.size() >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) s.erase(0,3);
    return Json(s).run();
}

TextExpansionExportResult textExpansionExportOpenKey(const std::vector<TextExpansionEntry>& entries) {
    TextExpansionExportResult r; r.ok = true;
    r.text = ";Compatible OpenKey Macro Data file for UniKey*** version=1 ***\n";
    for (const auto& e : entries) {
        if (e.trigger.empty() || e.replacement.empty() || lineBreak(e.trigger) || lineBreak(e.replacement) ||
            e.trigger.find(u':') != std::u16string::npos) { ++r.skipped; continue; }
        std::string a,b;
        if (!utf16To8(e.trigger,a) || !utf16To8(e.replacement,b)) { ++r.skipped; continue; }
        r.text += a + ":" + b + "\n"; ++r.exported;
    }
    return r;
}

TextExpansionIOResult textExpansionImportOpenKey(const std::string& text) {
    TextExpansionIOResult r; r.ok = true;
    std::unordered_map<std::u16string,std::size_t> idx;
    std::size_t p = 0; bool first = true;
    while (p <= text.size()) {
        std::size_t e = text.find('\n',p); if (e == std::string::npos) e = text.size();
        std::string line = text.substr(p,e-p); if (!line.empty() && line.back()=='\r') line.pop_back();
        if (first && line.size() >= 3 && (unsigned char)line[0]==0xEF && (unsigned char)line[1]==0xBB && (unsigned char)line[2]==0xBF) line.erase(0,3);
        bool header = first && line.rfind(";Compatible OpenKey Macro Data file for UniKey",0)==0; first = false;
        if (!line.empty() && !header) {
            std::size_t c = line.find(':'); if (c == 0) c = line.find(':',1);
            if (c == std::string::npos) ++r.skipped;
            else {
                std::u16string a,b;
                if (!utf8To16(line.substr(0,c),a) || !utf8To16(line.substr(c+1),b)) ++r.skipped;
                else add(r,{a,b},idx);
            }
        }
        if (e == text.size()) break; p = e + 1;
    }
    return r;
}

TextExpansionIOResult textExpansionImportLegacyOpenKeyBinary(const std::uint8_t* data, std::size_t size) {
    TextExpansionIOResult r;
    std::unordered_map<std::u16string,std::size_t> idx;
    if (!size) { r.ok = true; return r; }
    if (!data || size < 2) { r.error = "truncated OpenKey macro header"; return r; }
    std::size_t p = 0; std::uint16_t count = (std::uint16_t)(data[0] | ((std::uint16_t)data[1] << 8)); p = 2;
    for (std::uint16_t i = 0; i < count; ++i) {
        if (p >= size) { r.error = "truncated OpenKey macro trigger length"; return r; }
        std::size_t an = data[p++]; if (an > size-p) { r.error = "truncated OpenKey macro trigger"; return r; }
        std::string a8((const char*)data+p,an); p += an;
        if (size-p < 2) { r.error = "truncated OpenKey macro replacement length"; return r; }
        std::uint16_t bn = (std::uint16_t)(data[p] | ((std::uint16_t)data[p+1] << 8)); p += 2;
        if (bn > size-p) { r.error = "truncated OpenKey macro replacement"; return r; }
        std::string b8((const char*)data+p,bn); p += bn;
        std::u16string a,b;
        if (!utf8To16(a8,a) || !utf8To16(b8,b)) ++r.skipped; else add(r,{a,b},idx);
    }
    r.ok = true; return r;
}

} // namespace vikey
