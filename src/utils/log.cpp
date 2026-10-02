#include "log.h"
#include <cstdio>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <sys/stat.h>

namespace clak {
namespace utils {

constexpr long kMaxLogSizeBytes = 2 * 1024 * 1024;
static bool s_log_enabled = true;
static bool s_checked_env = false;

void setLogEnabled(bool enabled) {
    s_log_enabled = enabled;
    s_checked_env = true;
}

bool isLogEnabled() {
    if (!s_checked_env) {
        s_checked_env = true;
        const char* env = getenv("CLAK_LOG");
        if (env && (std::string(env) == "0" || std::string(env) == "false" || std::string(env) == "off")) {
            s_log_enabled = false;
        }
    }
    return s_log_enabled;
}

void clakLog(const std::string& msg) {
    if (!isLogEnabled()) return;

    const char* path = "/tmp/clak.log";
    struct stat st{};
    if (stat(path, &st) == 0 && st.st_size > kMaxLogSizeBytes) {
        // truncate when exceeding limit to keep file small
        FILE* trunc_fp = fopen(path, "w");
        if (trunc_fp) fclose(trunc_fp);
    }

    FILE* log_fp = fopen(path, "a");
    if (!log_fp) return;

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    struct tm tm_buf{};
    localtime_r(&t, &tm_buf);
    char ts[32];
    snprintf(ts, sizeof(ts), "%02d:%02d:%02d.%03ld",
             tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, ms);
    fprintf(log_fp, "[%s] %s\n", ts, msg.c_str());
    fclose(log_fp);
}

} // namespace utils
} // namespace clak
