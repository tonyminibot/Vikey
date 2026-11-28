//
//  Macro.h
//  Vikey
//
//  Gõ tắt (D68): a table of abbreviations the platform layer expands at the end of a word ("tks" ->
//  "cảm ơn"). Lives outside the Telex engine: the platform reads the word on screen, asks lookup(), and
//  if it matches deletes the word and types the expansion. Used by the macOS event tap and the Linux
//  IBus engine; the table is a plain text file, one "abbreviation:expansion" per line (UniKey/OpenKey
//  macro export format, so those files import as they are).
//

#ifndef VikeyMacro_h
#define VikeyMacro_h

#include <string>
#include <utility>
#include <vector>
#include "DataType.h"

namespace vikey { namespace macro {

/** Replace the table with the file's lines. Returns false (table empty) when the file can't be read. */
bool load(const std::string& path);
/** Replace the table with OpenKey's binary macro data (the "macroData" setting migrated from OpenKey). */
void loadOpenKeyBinary(const unsigned char* data, size_t size);
/** Write the table to the file (written whole, then renamed over the old one). */
bool save(const std::string& path);
/** Parse "abbr:expansion" lines (UTF-8, ';' starts a comment line) into the table, replacing it. */
void parse(const std::string& text);
std::string serialize();

/** Add or change one. The abbreviation has no spaces, the expansion is one line. False when either is empty or invalid. */
bool set(const std::string& abbreviation, const std::string& expansion);
bool remove(const std::string& abbreviation);
bool has(const std::string& abbreviation);
void clear();
size_t count();
/** All entries sorted by abbreviation (UTF-8). */
std::vector<std::pair<std::string, std::string>> all();

/** Whether the abbreviation is acceptable: letters and digits only, 1 to 32 characters. */
bool validAbbreviation(const std::string& abbreviation);

/**
 * The expansion for the word on screen (UTF-32), or false. An exact match wins. With autoCaps, "Tks" gives
 * "Cảm ơn" and "TKS" gives "CẢM ƠN" when only "tks" is in the table.
 */
bool lookup(const std::u32string& word, bool autoCaps, std::u32string& expansion);

/** Keys that end a word and expand it: Space, Return/Enter, Tab (also with Shift), and comma, period, semicolon (without). */
bool isTrigger(Uint16 key, bool shift);

/**
 * The word typed in English mode, kept here (letters and digits as typed): the English side of the platform
 * layer has no word buffer of its own when auto switch is off. Any other key, a click or a language change ends it.
 */
void englishType(Uint16 key, bool caps);
void englishBackspace();
void englishReset();
const std::u32string& englishWord();

std::string toUtf8(const std::u32string& s);
std::u32string fromUtf8(const std::string& s);

}}

#endif
