#include "TextExpansion.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using vikey::TextExpansionEntry;
using vikey::TextExpansionManager;
using vikey::TextExpansionSession;

static int failures = 0;

static void expect(bool ok, const char* name) {
    if (ok) return;
    std::cerr << "FAIL: " << name << "\n";
    failures++;
}

static void testBasicLookup() {
    TextExpansionManager m;
    m.setEntries({
        {u"dc", u"được"},
        {u"ss", u"Sun Santa"},
        {u"heart", u"❤️"},
        {u"sig", u"Harry Tran\nSun Santa"},
    });

    auto dc = m.find(u"dc", false);
    expect(dc.matched && dc.deleteCount == 2 && dc.replacement == u"được", "basic exact lookup");

    auto ss = m.find(u"ss", false);
    expect(ss.matched && ss.replacement == u"Sun Santa", "replacement preserves configured case");

    expect(!m.find(u"abcdc", false).matched, "does not match inside a larger token");
    expect(!m.find(u"d", false).matched, "does not match prefixes");
    expect(m.find(u"heart", false).replacement == u"❤️", "emoji replacement");
    expect(m.find(u"sig", false).replacement == u"Harry Tran\nSun Santa", "multiline replacement");
}

static void testMutation() {
    TextExpansionManager m;
    expect(!m.addOrUpdate({u"", u"x"}), "reject empty trigger");
    expect(!m.addOrUpdate({u"x", u""}), "reject empty replacement");
    expect(m.addOrUpdate({u"dc", u"được"}), "add entry");
    expect(m.addOrUpdate({u"dc", u"được rồi"}), "update entry");
    expect(m.entries().size() == 1 && m.find(u"dc", false).replacement == u"được rồi", "update does not duplicate entry");
    expect(!m.remove(u"missing"), "remove missing entry reports false");
    expect(m.remove(u"dc") && !m.find(u"dc", false).matched, "remove existing entry");

    m.setEntries({{u"", u"bad"}, {u"ok", u"good"}, {u"bad", u""}});
    expect(m.entries().size() == 1 && m.find(u"ok", false).matched, "setEntries skips invalid entries");
}

static void testAutoCaps() {
    TextExpansionManager m;
    m.setEntries({{u"dc", u"được"}, {u"ss", u"Sun Santa"}, {u"đc", u"được"}, {u"ýc", u"ý kiến"}});

    expect(!m.find(u"Dc", false).matched, "auto caps disabled is exact only");

    auto title = m.find(u"Dc", true);
    expect(title.matched && title.replacement == u"Được", "ASCII title case trigger uppercases Vietnamese replacement");

    auto upper = m.find(u"DC", true);
    expect(upper.matched && upper.replacement == u"ĐƯỢC", "all-caps trigger uppercases Vietnamese replacement");

    auto viTitle = m.find(u"Đc", true);
    expect(viTitle.matched && viTitle.replacement == u"Được", "Vietnamese title-case trigger");

    auto viUpper = m.find(u"ĐC", true);
    expect(viUpper.matched && viUpper.replacement == u"ĐƯỢC", "Vietnamese all-caps trigger");

    auto yTitle = m.find(u"Ýc", true);
    expect(yTitle.matched && yTitle.replacement == u"Ý kiến", "Vietnamese Ý title-case trigger");

    auto yUpper = m.find(u"ÝC", true);
    expect(yUpper.matched && yUpper.replacement == u"Ý KIẾN", "Vietnamese Ý all-caps trigger");

    expect(!m.find(u"dC", true).matched, "mixed-case trigger does not auto-match");

    auto brand = m.find(u"Ss", true);
    expect(brand.matched && brand.replacement == u"Sun Santa", "title case preserves configured replacement casing beyond first letter");
}

static void testExactCaseWins() {
    TextExpansionManager m;
    m.setEntries({{u"dc", u"được"}, {u"DC", u"District of Columbia"}});
    auto x = m.find(u"DC", true);
    expect(x.matched && x.replacement == u"District of Columbia", "exact case-specific entry wins over auto caps");
}

static void testLongReplacement() {
    std::u16string longText(200, u'x');
    TextExpansionManager m;
    m.setEntries({{u"long", longText}});
    auto match = m.find(u"long", false);
    expect(match.matched && match.replacement.size() == 200, "replacement is not limited by MAX_BUFF");
}

static void testSession() {
    TextExpansionSession s;
    s.append(u'd'); s.append(u'c');
    expect(s.token() == u"dc", "session append");

    s.erase(1);
    expect(s.token() == u"d", "session erase");

    s.applyEdit(1, u"đ");
    expect(s.token() == u"đ", "session applies visible engine edit");

    s.append(u"ược");
    expect(s.token() == u"được", "session tracks Vietnamese visible text");

    s.reset();
    s.append(u"ab");
    s.applyEdit(1, u"c");
    expect(s.token() == u"ac", "session edit replaces suffix");

    s.erase(99);
    expect(s.token().empty(), "session resets when delete exceeds known context");
}

static void testSurrogateHandling() {
    // U+1F600 GRINNING FACE as a UTF-16 surrogate pair.
    TextExpansionSession s;
    s.append(std::u16string{0xD83D, 0xDE00});
    s.append(u'x');
    s.erase(1);
    expect(s.token() == std::u16string({0xD83D, 0xDE00}), "erase removes one trailing scalar");
    s.erase(1);
    expect(s.token().empty(), "erase removes a surrogate pair as one scalar");

    TextExpansionManager m;
    m.setEntries({{std::u16string({0xD83D, 0xDE00}), u"smile"}});
    auto match = m.find(std::u16string({0xD83D, 0xDE00}), false);
    expect(match.matched && match.deleteCount == 1, "match delete count counts surrogate pair as one scalar");
}

int main() {
    testBasicLookup();
    testMutation();
    testAutoCaps();
    testExactCaseWins();
    testLongReplacement();
    testSession();
    testSurrogateHandling();

    if (failures) {
        std::cerr << failures << " text expansion test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "text expansion: all tests passed\n";
    return EXIT_SUCCESS;
}
