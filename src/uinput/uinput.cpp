#include "uinput.h"
#include "utils/log.h"
#include <sys/ioctl.h>
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

namespace clak {
namespace uinput {

UinputTool& UinputTool::instance() {
    static UinputTool inst;
    return inst;
}

UinputTool::UinputTool() {
    init_direct_uinput();
}

UinputTool::~UinputTool() {
    std::lock_guard<std::mutex> lock(uinput_mutex_);
    if (direct_fd_ >= 0) {
        ioctl(direct_fd_, UI_DEV_DESTROY);
        close(direct_fd_);
        direct_fd_ = -1;
    }
}

bool UinputTool::init_direct_uinput() {
    if (direct_fd_ >= 0) return true;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    int open_err = (fd < 0) ? errno : 0;
    utils::clakLog("uinput direct open(/dev/uinput): ret=" + std::to_string(fd) +
                   (fd < 0 ? (" errno=" + std::to_string(open_err) + " (" + strerror(open_err) + ")") : " (ok)"));
    if (fd < 0) return false;

    int ret1 = ioctl(fd, UI_SET_EVBIT, EV_KEY);
    int ret2 = ioctl(fd, UI_SET_KEYBIT, KEY_BACKSPACE);
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFTSHIFT);
    ioctl(fd, UI_SET_KEYBIT, KEY_LEFT);
    ioctl(fd, UI_SET_KEYBIT, KEY_DELETE);
    utils::clakLog("uinput direct ioctl UI_SET_KEYBIT: evbit_ret=" + std::to_string(ret1) +
                   " keybit_ret=" + std::to_string(ret2));

    struct uinput_setup usetup{};
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    strncpy(usetup.name, "Clak-Uinput", UINPUT_MAX_NAME_SIZE - 1);

    int setup_ret = ioctl(fd, UI_DEV_SETUP, &usetup);
    int create_ret = ioctl(fd, UI_DEV_CREATE);
    int create_err = (create_ret < 0) ? errno : 0;
    utils::clakLog("uinput direct ioctl UI_DEV_CREATE: setup_ret=" + std::to_string(setup_ret) +
                   " create_ret=" + std::to_string(create_ret) +
                   (create_ret < 0 ? (" errno=" + std::to_string(create_err) + " (" + strerror(create_err) + ")") : " (ok)"));

    if (create_ret < 0) {
        close(fd);
        return false;
    }
    direct_fd_ = fd;
    return true;
}

bool UinputTool::send_backspace_direct(size_t count, uint32_t post_delay_ms, uint32_t pre_delay_ms, uint32_t gap_ms) {
    if (direct_fd_ < 0 || count == 0) {
        return false;
    }

    // synchronous fast path when zero delays requested
    if (pre_delay_ms == 0 && post_delay_ms == 0 && gap_ms == 0) {
        std::lock_guard<std::mutex> lock(uinput_mutex_);
        if (direct_fd_ < 0) return false;

        std::vector<struct input_event> evs;
        evs.reserve(count * 4);
        for (size_t i = 0; i < count; ++i) {
            evs.push_back({ {}, EV_KEY, KEY_BACKSPACE, 1 });
            evs.push_back({ {}, EV_SYN, SYN_REPORT, 0 });
            evs.push_back({ {}, EV_KEY, KEY_BACKSPACE, 0 });
            evs.push_back({ {}, EV_SYN, SYN_REPORT, 0 });
        }
        ssize_t written = write(direct_fd_, evs.data(), evs.size() * sizeof(struct input_event));
        return written > 0;
    }

    // paced uinput worker thread prevents blocking fcitx event loop
    std::thread([this, count, post_delay_ms, pre_delay_ms, gap_ms]() {
        if (pre_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(pre_delay_ms));
        }

        auto emit_bs = [this]() {
            std::lock_guard<std::mutex> lock(uinput_mutex_);
            if (direct_fd_ < 0) return;
            struct input_event evs[4]{};
            evs[0] = { {}, EV_KEY, KEY_BACKSPACE, 1 };
            evs[1] = { {}, EV_SYN, SYN_REPORT, 0 };
            evs[2] = { {}, EV_KEY, KEY_BACKSPACE, 0 };
            evs[3] = { {}, EV_SYN, SYN_REPORT, 0 };
            ssize_t written = write(direct_fd_, evs, sizeof(evs));
            (void)written;
        };

        // send deletion backspaces before the sentinel
        for (size_t i = 0; i < count - 1; ++i) {
            emit_bs();
            if (gap_ms > 0 && i + 1 < count - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(gap_ms));
            }
        }

        // wait for target app to finish deletion before dispatching sentinel
        if (post_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(post_delay_ms));
        }

        // send final sentinel backspace
        emit_bs();
    }).detach();

    return true;
}

bool UinputTool::send_select(size_t count) {
    if (direct_fd_ < 0) {
        init_direct_uinput();
    }
    if (direct_fd_ < 0 || count == 0) return false;

    // run in a background thread so fcitx event loop stays non-blocking
    std::thread([this, count]() {
        std::lock_guard<std::mutex> lock(uinput_mutex_);
        if (direct_fd_ < 0) return;

        // shift down
        struct input_event shift_down[2]{};
        shift_down[0] = { {}, EV_KEY, KEY_LEFTSHIFT, 1 };
        shift_down[1] = { {}, EV_SYN, SYN_REPORT, 0 };
        (void)write(direct_fd_, shift_down, sizeof(shift_down));

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // count times left arrow down and up under shift
        for (size_t i = 0; i < count; ++i) {
            struct input_event left_ev[4]{};
            left_ev[0] = { {}, EV_KEY, KEY_LEFT, 1 };
            left_ev[1] = { {}, EV_SYN, SYN_REPORT, 0 };
            left_ev[2] = { {}, EV_KEY, KEY_LEFT, 0 };
            left_ev[3] = { {}, EV_SYN, SYN_REPORT, 0 };
            (void)write(direct_fd_, left_ev, sizeof(left_ev));
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // shift up
        struct input_event shift_up[2]{};
        shift_up[0] = { {}, EV_KEY, KEY_LEFTSHIFT, 0 };
        shift_up[1] = { {}, EV_SYN, SYN_REPORT, 0 };
        (void)write(direct_fd_, shift_up, sizeof(shift_up));

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // delete to erase selected text
        struct input_event del_ev[4]{};
        del_ev[0] = { {}, EV_KEY, KEY_DELETE, 1 };
        del_ev[1] = { {}, EV_SYN, SYN_REPORT, 0 };
        del_ev[2] = { {}, EV_KEY, KEY_DELETE, 0 };
        del_ev[3] = { {}, EV_SYN, SYN_REPORT, 0 };
        (void)write(direct_fd_, del_ev, sizeof(del_ev));

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // sentinel left arrow for fcitx completion
        struct input_event sentinel_ev[4]{};
        sentinel_ev[0] = { {}, EV_KEY, KEY_LEFT, 1 };
        sentinel_ev[1] = { {}, EV_SYN, SYN_REPORT, 0 };
        sentinel_ev[2] = { {}, EV_KEY, KEY_LEFT, 0 };
        sentinel_ev[3] = { {}, EV_SYN, SYN_REPORT, 0 };
        (void)write(direct_fd_, sentinel_ev, sizeof(sentinel_ev));
    }).detach();

    return true;
}

bool UinputTool::send_backspace(size_t count, uint32_t post_delay_ms, uint32_t pre_delay_ms, uint32_t gap_ms) {
    if (mock_handler_) {
        return mock_handler_(count, post_delay_ms, pre_delay_ms, gap_ms);
    }
    if (direct_fd_ < 0) {
        init_direct_uinput();
    }
    return send_backspace_direct(count, post_delay_ms, pre_delay_ms, gap_ms);
}

} // namespace uinput
} // namespace clak
