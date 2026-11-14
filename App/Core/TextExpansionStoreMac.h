#pragma once

#include "TextExpansionIO.h"
#include <string>

namespace vikey { namespace text_expansion_store_mac {

/// ~/Library/Application Support/Vikey/text-expansions.json
std::string path();

/// Missing file is a successful empty list. Malformed/oversized files fail safely.
TextExpansionIOResult load();

}} // namespace
