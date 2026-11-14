// Windows storage bridge for text expansion. Phase 3 is read-only; the settings UI adds writes later.
#pragma once

#include "TextExpansionIO.h"
#include <string>

namespace vikey { namespace text_expansion_store {

/// %LOCALAPPDATA%\Vikey\text-expansions.json
std::wstring path();

/// Missing file is a successful empty list. Malformed/oversized files fail safely.
TextExpansionIOResult load();

}} // namespace
