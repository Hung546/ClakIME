#ifndef CLAK_PLATFORM_WINDOW_INFO_H
#define CLAK_PLATFORM_WINDOW_INFO_H

#include <string>
#include <sys/types.h>

namespace clak {
namespace platform {

struct WindowInfo {
    std::string win_class;
    std::string win_title;
    pid_t pid{0};
};

WindowInfo getActiveWindow();

} // namespace platform
} // namespace clak

#endif
