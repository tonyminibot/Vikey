//
//  TextExpansionStore.mm
//  Vikey
//

#import <Foundation/Foundation.h>
#include "TextExpansionStore.h"

namespace vikey { namespace text_expansion_store {

static const NSUInteger MAX_TEXT_EXPANSION_FILE = 4u * 1024u * 1024u;

static NSString* Directory() {
    NSString* base = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject;
    NSString* dir = [base stringByAppendingPathComponent:@"Vikey"];
    [[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:nil];
    return dir;
}

static NSString* FilePath() {
    return [Directory() stringByAppendingPathComponent:@"text-expansions.json"];
}

std::string path() {
    return std::string(FilePath().UTF8String ?: "");
}

TextExpansionIOResult load() {
    TextExpansionIOResult r;
    NSString* p = FilePath();
    NSFileManager* fm = [NSFileManager defaultManager];
    if (![fm fileExistsAtPath:p]) { r.ok = true; return r; }

    NSDictionary* attrs = [fm attributesOfItemAtPath:p error:nil];
    unsigned long long size = [attrs fileSize];
    if (size > MAX_TEXT_EXPANSION_FILE) {
        r.error = "text-expansions.json is too large";
        return r;
    }
    if (size == 0) { r.ok = true; return r; }

    NSData* data = [NSData dataWithContentsOfFile:p options:NSDataReadingMappedIfSafe error:nil];
    if (!data || data.length != size) {
        r.error = "cannot read text-expansions.json";
        return r;
    }
    std::string json((const char*)data.bytes, data.length);
    return textExpansionParseJson(json);
}

bool save(const std::vector<TextExpansionEntry>& entries, std::string* error) {
    std::string json = textExpansionSerializeJson(entries);
    NSData* data = [NSData dataWithBytes:json.data() length:json.size()];
    NSString* dst = FilePath();
    NSString* tmp = [dst stringByAppendingString:@".tmp"];

    NSError* err = nil;
    if (![data writeToFile:tmp options:NSDataWritingAtomic error:&err]) {
        if (error) *error = "cannot write temporary text-expansions.json";
        return false;
    }

    NSFileManager* fm = [NSFileManager defaultManager];
    if ([fm fileExistsAtPath:dst]) {
        NSURL* resulting = nil;
        if (![fm replaceItemAtURL:[NSURL fileURLWithPath:dst]
                    withItemAtURL:[NSURL fileURLWithPath:tmp]
                   backupItemName:nil
                          options:0
                 resultingItemURL:&resulting
                            error:&err]) {
            [fm removeItemAtPath:tmp error:nil];
            if (error) *error = "cannot replace text-expansions.json";
            return false;
        }
    } else if (![fm moveItemAtPath:tmp toPath:dst error:&err]) {
        [fm removeItemAtPath:tmp error:nil];
        if (error) *error = "cannot create text-expansions.json";
        return false;
    }
    return true;
}

}} // namespace
