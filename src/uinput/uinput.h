#ifndef CLAK_UINPUT_UINPUT_H
#define CLAK_UINPUT_UINPUT_H

#include <cstddef>
#include <cstdint>
#include <mutex>

namespace clak {
namespace uinput {

class UinputTool {
public:
    static UinputTool& instance();

    bool send_backspace(size_t count, uint32_t post_delay_ms = 0, uint32_t pre_delay_ms = 0, uint32_t gap_ms = 0);
    bool send_select(size_t count);

private:
    UinputTool();
    ~UinputTool();

    bool init_direct_uinput();
    bool send_backspace_direct(size_t count, uint32_t post_delay_ms, uint32_t pre_delay_ms, uint32_t gap_ms);

    int direct_fd_{-1};
    std::mutex uinput_mutex_;
};

} // namespace uinput
} // namespace clak

#endif
