//
//  TextExpansion.cpp
//  Vikey
//

#include "TextExpansion.h"

#include <algorithm>

namespace vikey {
namespace {

static char16_t lowerVietnamese(char16_t c) {
    if (c >= u'A' && c <= u'Z') return (char16_t)(c + (u'a' - u'A'));

    switch (c) {
        case u'À': return u'à'; case u'Á': return u'á'; case u'Â': return u'â'; case u'Ã': return u'ã';
        case u'È': return u'è'; case u'É': return u'é'; case u'Ê': return u'ê';
        case u'Ì': return u'ì'; case u'Í': return u'í';
        case u'Ò': return u'ò'; case u'Ó': return u'ó'; case u'Ô': return u'ô'; case u'Õ': return u'õ';
        case u'Ù': return u'ù'; case u'Ú': return u'ú'; case u'Ý': return u'ý';
        case u'Ă': return u'ă'; case u'Đ': return u'đ'; case u'Ĩ': return u'ĩ'; case u'Ũ': return u'ũ';
        case u'Ơ': return u'ơ'; case u'Ư': return u'ư';
        default: break;
    }

    // Vietnamese precomposed letters U+1EA0..U+1EF9 are uppercase/lowercase pairs.
    if (c >= 0x1EA0 && c <= 0x1EF8 && ((c - 0x1EA0) % 2 == 0)) return (char16_t)(c + 1);
    return c;
}

static char16_t upperVietnamese(char16_t c) {
    if (c >= u'a' && c <= u'z') return (char16_t)(c - (u'a' - u'A'));

    switch (c) {
        case u'à': return u'À'; case u'á': return u'Á'; case u'â': return u'Â'; case u'ã': return u'Ã';
        case u'è': return u'È'; case u'é': return u'É'; case u'ê': return u'Ê';
        case u'ì': return u'Ì'; case u'í': return u'Í';
        case u'ò': return u'Ò'; case u'ó': return u'Ó'; case u'ô': return u'Ô'; case u'õ': return u'Õ';
        case u'ù': return u'Ù'; case u'ú': return u'Ú'; case u'ý': return u'Ý';
        case u'ă': return u'Ă'; case u'đ': return u'Đ'; case u'ĩ': return u'Ĩ'; case u'ũ': return u'Ũ';
        case u'ơ': return u'Ơ'; case u'ư': return u'Ư';
        default: break;
    }

    if (c >= 0x1EA1 && c <= 0x1EF9 && ((c - 0x1EA1) % 2 == 0)) return (char16_t)(c - 1);
    return c;
}

static bool isCaseable(char16_t c) {
    return lowerVietnamese(c) != upperVietnamese(c);
}

static std::u16string lowerString(const std::u16string& s) {
    std::u16string out = s;
    for (char16_t& c : out) c = lowerVietnamese(c);
    return out;
}

static void upperAll(std::u16string& s) {
    for (char16_t& c : s) c = upperVietnamese(c);
}

static void upperFirstCaseable(std::u16string& s) {
    for (char16_t& c : s) {
        if (!isCaseable(c)) continue;
        c = upperVietnamese(c);
        return;
    }
}

enum class CaseStyle { Lower, Title, Upper, Mixed };

static CaseStyle caseStyle(const std::u16string& s) {
    bool sawCaseable = false;
    bool allUpper = true;
    bool allLower = true;
    bool title = true;
    bool sawFirstCaseable = false;

    for (char16_t c : s) {
        if (!isCaseable(c)) continue;
        sawCaseable = true;
        bool isUpper = c == upperVietnamese(c) && c != lowerVietnamese(c);
        bool isLower = c == lowerVietnamese(c) && c != upperVietnamese(c);
        allUpper = allUpper && isUpper;
        allLower = allLower && isLower;
        if (!sawFirstCaseable) {
            title = title && isUpper;
            sawFirstCaseable = true;
        } else {
            title = title && isLower;
        }
    }

    if (!sawCaseable || allLower) return CaseStyle::Lower;
    if (allUpper) return CaseStyle::Upper;
    if (title) return CaseStyle::Title;
    return CaseStyle::Mixed;
}

static int unicodeScalarCount(const std::u16string& s) {
    int count = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char16_t c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) {
            char16_t n = s[i + 1];
            if (n >= 0xDC00 && n <= 0xDFFF) ++i;
        }
        ++count;
    }
    return count;
}

} // namespace

void TextExpansionManager::rebuildIndexes() {
    exactIndex_.clear();
    lowerIndex_.clear();

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto& trigger = entries_[i].trigger;
        exactIndex_[trigger] = i;

        // Auto-caps fallback is intentionally based only on lowercase configured triggers.
        // Exact case-sensitive entries still win above, so users can define explicit case variants.
        std::u16string lower = lowerString(trigger);
        if (lower == trigger) lowerIndex_[lower] = i;
    }
}

void TextExpansionManager::setEntries(const std::vector<TextExpansionEntry>& entries) {
    clear();
    for (const auto& entry : entries) addOrUpdate(entry);
}

bool TextExpansionManager::addOrUpdate(const TextExpansionEntry& entry) {
    if (entry.trigger.empty() || entry.replacement.empty()) return false;

    auto it = exactIndex_.find(entry.trigger);
    if (it != exactIndex_.end()) {
        entries_[it->second].replacement = entry.replacement;
        rebuildIndexes();
        return true;
    }

    entries_.push_back(entry);
    rebuildIndexes();
    return true;
}

bool TextExpansionManager::remove(const std::u16string& trigger) {
    auto it = exactIndex_.find(trigger);
    if (it == exactIndex_.end()) return false;
    entries_.erase(entries_.begin() + (std::ptrdiff_t)it->second);
    rebuildIndexes();
    return true;
}

void TextExpansionManager::clear() {
    entries_.clear();
    exactIndex_.clear();
    lowerIndex_.clear();
}

TextExpansionMatch TextExpansionManager::find(const std::u16string& visibleToken, bool autoCaps) const {
    TextExpansionMatch result;
    if (visibleToken.empty()) return result;

    auto exact = exactIndex_.find(visibleToken);
    if (exact != exactIndex_.end()) {
        result.matched = true;
        result.deleteCount = unicodeScalarCount(visibleToken);
        result.replacement = entries_[exact->second].replacement;
        return result;
    }

    if (!autoCaps) return result;

    CaseStyle style = caseStyle(visibleToken);
    if (style == CaseStyle::Mixed) return result;

    std::u16string lower = lowerString(visibleToken);
    auto it = lowerIndex_.find(lower);
    if (it == lowerIndex_.end()) return result;

    result.matched = true;
    result.deleteCount = unicodeScalarCount(visibleToken);
    result.replacement = entries_[it->second].replacement;

    if (style == CaseStyle::Upper) upperAll(result.replacement);
    else if (style == CaseStyle::Title) upperFirstCaseable(result.replacement);

    return result;
}

void TextExpansionSession::erase(int deleteCount) {
    if (deleteCount <= 0) return;

    while (deleteCount-- > 0) {
        if (token_.empty()) {
            token_.clear();
            return;
        }

        char16_t last = token_.back();
        token_.pop_back();
        if (last >= 0xDC00 && last <= 0xDFFF && !token_.empty()) {
            char16_t first = token_.back();
            if (first >= 0xD800 && first <= 0xDBFF) token_.pop_back();
        }
    }
}

void TextExpansionSession::applyEdit(int deleteCount, const std::u16string& insertedText) {
    erase(deleteCount);
    token_ += insertedText;
}

} // namespace vikey
