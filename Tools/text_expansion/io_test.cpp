#include "TextExpansionIO.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace vikey;

static int failures = 0;
static void expect(bool ok, const char* name) {
    if (ok) return;
    std::cerr << "FAIL: " << name << "\n";
    ++failures;
}

static bool same(const std::vector<TextExpansionEntry>& a, const std::vector<TextExpansionEntry>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].trigger != b[i].trigger || a[i].replacement != b[i].replacement) return false;
    return true;
}

static std::vector<std::uint8_t> legacy(const std::vector<std::pair<std::string,std::string>>& entries) {
    std::vector<std::uint8_t> out;
    std::uint16_t count = (std::uint16_t)entries.size();
    out.push_back((std::uint8_t)(count & 0xFF)); out.push_back((std::uint8_t)(count >> 8));
    for (const auto& e : entries) {
        out.push_back((std::uint8_t)e.first.size());
        out.insert(out.end(), e.first.begin(), e.first.end());
        std::uint16_t n = (std::uint16_t)e.second.size();
        out.push_back((std::uint8_t)(n & 0xFF)); out.push_back((std::uint8_t)(n >> 8));
        out.insert(out.end(), e.second.begin(), e.second.end());
    }
    return out;
}

static void testJson() {
    std::vector<TextExpansionEntry> src = {
        {u"dc", u"được"},
        {u"ss", u"Sun Santa"},
        {u"heart", u"❤️"},
        {u"sig", u"Harry \"H\" \\ Tran\nSun Santa\t✓"},
    };
    auto r = textExpansionParseJson(textExpansionSerializeJson(src));
    expect(r.ok && same(src, r.entries), "JSON round trip preserves Unicode/emoji/escapes");
    expect(r.imported == 4 && r.replaced == 0 && r.skipped == 0, "JSON round trip counts");

    auto dup = textExpansionParseJson(
        "{\"unknown\":{\"a\":[1,true,null]},\"version\":1,\"entries\":["
        "{\"trigger\":\"dc\",\"replacement\":\"one\",\"extra\":42},"
        "{\"trigger\":\"dc\",\"replacement\":\"two\"},"
        "{\"trigger\":\"\",\"replacement\":\"bad\"},"
        "{\"trigger\":9,\"replacement\":\"bad\"},42]}");
    expect(dup.ok && dup.entries.size() == 1 && dup.entries[0].replacement == u"two", "JSON duplicate is last-wins");
    expect(dup.imported == 1 && dup.replaced == 1 && dup.skipped == 3, "JSON duplicate/skipped counts");

    expect(!textExpansionParseJson("{\"version\":2,\"entries\":[]}").ok, "JSON rejects unsupported version");
    expect(!textExpansionParseJson("{\"version\":1}").ok, "JSON requires entries");
    expect(!textExpansionParseJson("{\"version\":1,\"entries\":[}").ok, "JSON rejects malformed syntax");
    expect(!textExpansionParseJson("{\"version\":1,\"entries\":[{\"trigger\":\"x\",\"replacement\":\"\\uD83D\"}]}").ok,
           "JSON rejects unpaired surrogate");
    expect(!textExpansionParseJson("{\"version\":1,\"entries\":[],\"bad\":1+2}").ok,
           "JSON rejects malformed number in unknown field");
    std::string bom = "\xEF\xBB\xBF{\"version\":1,\"entries\":[]}";
    expect(textExpansionParseJson(bom).ok, "JSON accepts UTF-8 BOM");
}

static void testOpenKeyText() {
    std::vector<TextExpansionEntry> src = {
        {u"dc", u"được"},
        {u"ph", u"phụ huynh: lớp 2"},
        {u"heart", u"❤️"},
        {u"multi", u"line 1\nline 2"},
        {u":abc", u"leading colon"},
        {u"a:b", u"not representable"},
    };
    auto ex = textExpansionExportOpenKey(src);
    expect(ex.ok && ex.exported == 4 && ex.skipped == 2, "OpenKey export counts unsupported lines");
    auto r = textExpansionImportOpenKey(ex.text);
    expect(r.ok && r.entries.size() == 4, "OpenKey export imports back");
    expect(r.entries[0].replacement == u"được", "OpenKey Vietnamese round trip");
    expect(r.entries[1].replacement == u"phụ huynh: lớp 2", "OpenKey preserves colon in replacement");
    expect(r.entries[2].replacement == u"❤️", "OpenKey preserves emoji");
    expect(r.entries[3].trigger == u":abc" && r.entries[3].replacement == u"leading colon",
           "OpenKey export/import preserves leading-colon trigger");

    auto crlf = textExpansionImportOpenKey(
        ";Compatible OpenKey Macro Data file for UniKey*** version=1 ***\r\n"
        "dc:one\r\ndc:two\r\n:abc:value\r\ninvalid\r\n");
    expect(crlf.ok && crlf.entries.size() == 2, "OpenKey accepts CRLF");
    expect(crlf.entries[0].replacement == u"two", "OpenKey duplicate is last-wins");
    expect(crlf.entries[1].trigger == u":abc" && crlf.entries[1].replacement == u"value",
           "OpenKey leading-colon trigger compatibility");
    expect(crlf.imported == 2 && crlf.replaced == 1 && crlf.skipped == 1, "OpenKey import counts");

    std::string bad = "ok:yes\nbad:";
    bad.push_back((char)0xC0); bad.push_back((char)0xAF); bad += "\n";
    auto invalid = textExpansionImportOpenKey(bad);
    expect(invalid.ok && invalid.entries.size() == 1 && invalid.skipped == 1, "OpenKey skips invalid UTF-8");
}

static void testLegacyBinary() {
    auto data = legacy({
        {"dc", "được"},
        {"ss", "Sun Santa"},
        {"dc", "được rồi"},
        {"heart", "❤️"},
    });
    auto r = textExpansionImportLegacyOpenKeyBinary(data.data(), data.size());
    expect(r.ok && r.entries.size() == 3, "legacy binary parses and deduplicates");
    expect(r.entries[0].trigger == u"dc" && r.entries[0].replacement == u"được rồi",
           "legacy binary duplicate is last-wins");
    expect(r.imported == 3 && r.replaced == 1 && r.skipped == 0, "legacy binary counts");

    auto empty = textExpansionImportLegacyOpenKeyBinary(nullptr, 0);
    expect(empty.ok && empty.entries.empty(), "legacy empty registry value is safe");

    for (std::size_t n = 1; n < data.size(); ++n) {
        auto cut = textExpansionImportLegacyOpenKeyBinary(data.data(), n);
        expect(!cut.ok, "legacy truncation fails safely");
    }

    auto badUtf8 = legacy({{"ok","yes"},{"bad",std::string("\xC0\xAF",2)}});
    auto invalid = textExpansionImportLegacyOpenKeyBinary(badUtf8.data(), badUtf8.size());
    expect(invalid.ok && invalid.entries.size() == 1 && invalid.skipped == 1, "legacy invalid UTF-8 entry is skipped");
}

int main() {
    testJson();
    testOpenKeyText();
    testLegacyBinary();
    if (failures) {
        std::cerr << failures << " text expansion IO test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "text expansion IO: all tests passed\n";
    return EXIT_SUCCESS;
}
