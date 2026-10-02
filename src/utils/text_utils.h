#ifndef CLAK_UTILS_TEXT_UTILS_H
#define CLAK_UTILS_TEXT_UTILS_H

#include <string>
#include <cstddef>

namespace clak {
namespace utils {

void popUtf8Chars(std::string& s, size_t count);
std::string extractWordBeforeCursor(const std::string& text, size_t cursor_chars);

} // namespace utils
} // namespace clak

#endif
