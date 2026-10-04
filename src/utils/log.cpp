#include "log.h"
#include <cstdio>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

namespace clak {
namespace utils {

constexpr off_t kMaxLogSizeBytes = 2 * 1024 * 1024;
static bool s_log_enabled = false;
static bool s_checked_env = false;

void setLogEnabled(bool enabled) {
    s_log_enabled = enabled;
    s_checked_env = true;
}

bool isLogEnabled() {
    if (!s_checked_env) {
        s_checked_env = true;
        const char* env = getenv("CLAK_LOG");
        if (env) {
            s_log_enabled = (std::string(env) != "0" && std::string(env) != "false" && std::string(env) != "off");
        }
    }
    return s_log_enabled;
}

void clakLog(const std::string& msg) {
    if (!isLogEnabled()) return;

    const char* path = "/tmp/clak.log";
    // open with nofollow and strict user permissions to mitigate symlink and info disclosure attacks
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return;

    struct stat st{};
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != getuid()) {
        close(fd);
        return;
    }

    if (st.st_size > kMaxLogSizeBytes) {
        // truncate atomically on open fd without reopening path
        if (ftruncate(fd, 0) != 0) {
            close(fd);
            return;
        }
    }

    FILE* log_fp = fdopen(fd, "a");
    if (!log_fp) {
        close(fd);
        return;
    }

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
