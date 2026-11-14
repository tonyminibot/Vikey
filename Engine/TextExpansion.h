//
//  TextExpansion.h
//  Vikey
//
//  Shared, platform-independent text expansion model and current-token session.
//

#ifndef TextExpansion_h
#define TextExpansion_h

#include <string>
#include <unordered_map>
#include <vector>

namespace vikey {

struct TextExpansionEntry {
    std::u16string trigger;
    std::u16string replacement;
};

struct TextExpansionOptions {
    bool enabled = true;
    bool inEnglishMode = true;
    bool autoCaps = false;
};

struct TextExpansionMatch {
    bool matched = false;
    int deleteCount = 0;
    std::u16string replacement;
};

class TextExpansionManager {
public:
    void setEntries(const std::vector<TextExpansionEntry>& entries);
    const std::vector<TextExpansionEntry>& entries() const { return entries_; }

    bool addOrUpdate(const TextExpansionEntry& entry);
    bool remove(const std::u16string& trigger);
    void clear();

    TextExpansionMatch find(const std::u16string& visibleToken, bool autoCaps) const;

private:
    std::vector<TextExpansionEntry> entries_;
    std::unordered_map<std::u16string, std::size_t> exactIndex_;
    std::unordered_map<std::u16string, std::size_t> lowerIndex_;

    void rebuildIndexes();
};

class TextExpansionSession {
public:
    void reset() { token_.clear(); }
    void append(char16_t ch) { token_.push_back(ch); }
    void append(const std::u16string& text) { token_ += text; }

    // Remove visible Unicode scalar values from the end of the current token.
    // If deleteCount exceeds the known token, context is no longer trustworthy and is reset.
    void erase(int deleteCount);

    // Apply the same edit Vikey applied to the visible current token.
    void applyEdit(int deleteCount, const std::u16string& insertedText);

    const std::u16string& token() const { return token_; }

private:
    std::u16string token_;
};

} // namespace vikey

#endif /* TextExpansion_h */
