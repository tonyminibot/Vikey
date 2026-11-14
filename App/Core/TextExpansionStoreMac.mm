#include "TextExpansionStoreMac.h"

#import <Foundation/Foundation.h>

namespace vikey { namespace text_expansion_store_mac {

static const unsigned long long MAX_TEXT_EXPANSION_FILE = 4ull * 1024ull * 1024ull;

std::string path() {
    NSString* dir = [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject
                     stringByAppendingPathComponent:@"Vikey"];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    NSString* p = [dir stringByAppendingPathComponent:@"text-expansions.json"];
    return std::string(p.fileSystemRepresentation ?: "");
}

TextExpansionIOResult load() {
    TextExpansionIOResult r;
    std::string p = path();
    if (p.empty()) { r.error = "cannot resolve text-expansions.json path"; return r; }

    NSString* ns = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:p.c_str() length:p.size()];
    NSDictionary* attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:ns error:nil];
    if (!attrs) { r.ok = true; return r; }
    unsigned long long size = [attrs fileSize];
    if (size > MAX_TEXT_EXPANSION_FILE) { r.error = "text-expansions.json is too large"; return r; }
    if (size == 0) { r.ok = true; return r; }

    NSData* data = [NSData dataWithContentsOfFile:ns options:NSDataReadingMappedIfSafe error:nil];
    if (!data) { r.error = "cannot read text-expansions.json"; return r; }
    std::string json((const char*)data.bytes, data.length);
    return textExpansionParseJson(json);
}

}} // namespace
