//
//  TextExpansionStore.h
//  Vikey
//
//  macOS storage bridge for the shared text-expansion JSON format.
//

#pragma once

#include "TextExpansionIO.h"
#include <string>

namespace vikey { namespace text_expansion_store {

/// ~/Library/Application Support/Vikey/text-expansions.json
std::string path();

/// Missing file is a successful empty list. Malformed/oversized files fail safely.
TextExpansionIOResult load();

/// Atomic native-store write, used by the Phase 6 UI.
bool save(const std::vector<TextExpansionEntry>& entries, std::string* error = nullptr);

}} // namespace
