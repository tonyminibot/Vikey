//
//  TextExpansionIO.h
//  Vikey
//
//  Shared serialization and OpenKey/UniKey compatibility for text expansion.
//

#ifndef TextExpansionIO_h
#define TextExpansionIO_h

#include "TextExpansion.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vikey {

struct TextExpansionIOResult {
    bool ok = false;
    std::vector<TextExpansionEntry> entries;
    int imported = 0;
    int replaced = 0;
    int skipped = 0;
    std::string error;
};

struct TextExpansionExportResult {
    bool ok = false;
    std::string text;
    int exported = 0;
    int skipped = 0;
    std::string error;
};

// Vikey native storage format (UTF-8 JSON, schema version 1).
std::string textExpansionSerializeJson(const std::vector<TextExpansionEntry>& entries);
TextExpansionIOResult textExpansionParseJson(const std::string& json);

// OpenKey / UniKey compatible text macro format.
TextExpansionExportResult textExpansionExportOpenKey(const std::vector<TextExpansionEntry>& entries);
TextExpansionIOResult textExpansionImportOpenKey(const std::string& text);

// OpenKey's legacy HKCU\SOFTWARE\TuyenMai\OpenKey\macroData binary format.
TextExpansionIOResult textExpansionImportLegacyOpenKeyBinary(const std::uint8_t* data, std::size_t size);

} // namespace vikey

#endif /* TextExpansionIO_h */
