#include "window_info.h"
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <chrono>

namespace clak {
namespace platform {

WindowInfo getActiveWindow() {
    static WindowInfo s_cached_info;
    static int64_t s_last_ms = 0;
    static std::string s_sock_path;

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // cache window info for 500ms to avoid querying hyprland on every keystroke
    if (now_ms - s_last_ms < 500 && !s_cached_info.win_class.empty()) {
        return s_cached_info;
    }
    s_last_ms = now_ms;

    WindowInfo info;
    const char* xdg = getenv("XDG_RUNTIME_DIR");
    if (!xdg) return info;

    if (s_sock_path.empty()) {
        std::string sig;
        const char* env_sig = getenv("HYPRLAND_INSTANCE_SIGNATURE");
        if (env_sig && env_sig[0] != '\0') {
            sig = env_sig;
        } else {
            std::string hypr_dir = std::string(xdg) + "/hypr";
            DIR* d = opendir(hypr_dir.c_str());
            if (d) {
                struct dirent* ent;
                while ((ent = readdir(d)) != nullptr) {
                    if (ent->d_name[0] != '.') {
                        sig = ent->d_name;
                        break;
                    }
                }
                closedir(d);
            }
        }
        if (sig.empty()) return info;
        s_sock_path = std::string(xdg) + "/hypr/" + sig + "/.socket.sock";
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) return info;

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, s_sock_path.c_str(), sizeof(addr.sun_path) - 1);
    socklen_t len = offsetof(struct sockaddr_un, sun_path) + s_sock_path.length() + 1;

    if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), len) != 0) {
        close(fd);
        s_sock_path.clear();
        return info;
    }

    const char* cmd = "j/activewindow";
    send(fd, cmd, strlen(cmd), MSG_NOSIGNAL);

    std::string response;
    char buf[4096];
    struct pollfd pfd{fd, POLLIN, 0};
    if (poll(&pfd, 1, 15) > 0) {
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            response.assign(buf, n);
        }
    }
    close(fd);

    const char* p = strstr(response.c_str(), "\"pid\":");
    if (p) {
        p += 6;
        while (*p == ' ' || *p == '\t') p++;
        info.pid = static_cast<pid_t>(atoi(p));
    }

    p = strstr(response.c_str(), "\"class\":");
    if (p) {
        p += 8;
        while (*p == ' ' || *p == '\t' || *p == '\"') p++;
        const char* end = strchr(p, '\"');
        if (end) info.win_class = std::string(p, end - p);
    }

    p = strstr(response.c_str(), "\"title\":");
    if (p) {
        p += 8;
        while (*p == ' ' || *p == '\t' || *p == '\"') p++;
        const char* end = strchr(p, '\"');
        if (end) info.win_title = std::string(p, end - p);
    }

    if (!info.win_class.empty()) {
        s_cached_info = info;
    }
    return s_cached_info.win_class.empty() ? info : s_cached_info;
}

} // namespace platform
} // namespace clak
