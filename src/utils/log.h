#ifndef CLAK_UTILS_LOG_H
#define CLAK_UTILS_LOG_H

#include <string>

namespace clak {
namespace utils {

void clakLog(const std::string& msg);
void setLogEnabled(bool enabled);
bool isLogEnabled();

} // namespace utils
} // namespace clak

#endif
