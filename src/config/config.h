#ifndef CLAK_CONFIG_CONFIG_H
#define CLAK_CONFIG_CONFIG_H

#include <cstddef>
#include <cstdint>

namespace clak {
namespace config {

constexpr size_t kMaxBufferedKeys = 50;
constexpr int kMismatchThreshold = 3;
constexpr uint64_t kSafetyTimeoutUs = 50000;
constexpr uint32_t kAddressBarPostDelayMs = 20;
constexpr uint64_t kCacheCheckIntervalUs = 500000;

} // namespace config
} // namespace clak

#endif
