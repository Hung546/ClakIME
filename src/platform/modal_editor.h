#ifndef CLAK_PLATFORM_MODAL_EDITOR_H
#define CLAK_PLATFORM_MODAL_EDITOR_H

#include "window_info.h"

namespace clak {
namespace platform {

bool isEditorActive(const WindowInfo& win);
bool isAnyTerminalForeground();

} // namespace platform
} // namespace clak

#endif
