#pragma once
#include <windows.h>

namespace vikey { namespace ui {

/// Opens the Windows text-expansion manager. Only one instance is shown.
void showTextExpansionSettings(HINSTANCE inst, HWND owner = nullptr);

}} // namespace
