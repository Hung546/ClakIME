# Module Điều hướng Trạng thái Gõ (IME State & Dispatch)

Thư mục: [src/ime/](./src/ime/)

File chính: [state.h](./src/ime/state.h), [state.cpp](./src/ime/state.cpp)

Đây là khối điều khiển trung tâm của Clak trên Fcitx5, quản lý toàn bộ vòng đời phím bấm, quyết định sử dụng kênh Wayland SurroundingText hay Uinput, và đảm bảo tính toàn vẹn (correctness) của văn bản hiển thị.

---

## 1. Cơ chế Quyết định Kênh Xóa (shouldUseUinput)

Trên Wayland, các ứng dụng có cách cài đặt giao thức `zwp_text_input_v3` rất khác nhau:

- **Chromium / Chrome / Brave**: Hỗ trợ `delete_surrounding_text` cực tốt, độ trễ < 1ms. Clak dùng kênh SurroundingText trực tiếp.
- **Gecko (Firefox / Zen Browser)**: Lỗi xóa xung quanh kéo dài nhiều năm trên Wayland (DOM không cập nhật kịp thời, text bị stale). Clak bắt buộc điều hướng sang kênh **Uinput**.
- **Google Docs (Canvas Editor)**: Không dùng DOM HTML thông thường mà vẽ chữ lên HTML5 Canvas, surrounding text chỉ là 2 dấu cách giả lập (`  `). Clak bắt buộc điều hướng sang **Uinput**.
- **Terminal (Kitty, Alacritty, Foot)**: Terminal không hỗ trợ xóa lùi ngữ cảnh Wayland và nuốt sự kiện `forwardKey` khi IME kích hoạt. Clak dùng **Uinput**.
- **Address Bar (Thanh địa chỉ URL)**: Khi xuất hiện gợi ý tự động (autofill), con trỏ bị bôi đen hoặc nhảy về cuối. Clak dùng Uinput kèm thuật toán đếm bù 1 ký tự gợi ý.

```cpp
bool ClakState::shouldUseUinput(bool use_surrounding, uint32_t action_type) {
    std::string app = appKey();
    std::string site = activeSite();

    if (config::isTerminalApp(app)) return true;
    if (config::isGeckoApp(app)) return true;
    if (action_type == CLAK_ACTION_ADDRESS_BAR_FIX) return true;
    if (config::isForceUinputSite(site)) return true;
    if (is_canvas_editor_ || is_rich_text_editor_) return true;
    if (config::isMetaSite(site)) return false;
    return !use_surrounding;
}
```

---

## 2. Kỹ thuật Phím Chốt (Sentinel Backspace)

Khi Clak gửi phím BackSpace qua `/dev/uinput`, sự kiện này đi vào nhân Linux, chuyển qua compositor Hyprland, rồi mới đến ứng dụng đích và vòng ngược lại Fcitx5.

Nếu Clak gửi lệnh `commitString` ngay lập tức, chữ mới sẽ đến ứng dụng **trước** khi các phím BackSpace kịp xóa chữ cũ, gây ra hiện tượng nhân đôi chữ (ví dụ: `d` + commit `đ` = `dđ`).

Để giải quyết bài toán bất đồng bộ này, Clak áp dụng **Sentinel Backspace Protocol**:

1. Giả sử cần xóa $N$ ký tự thật. Clak sẽ phát qua Uinput tổng cộng $N + 1$ phím BackSpace.
2. $N$ phím đầu tiên là phím xóa thật, đến ứng dụng để xóa ký tự trong DOM/buffer.
3. Phím thứ $N + 1$ là **phím chốt (sentinel)**.
4. Khi phím thứ $N + 1$ vòng lặp lại hàm `keyEvent` của Clak:
    - Clak nuốt phím này (`keyEvent.filterAndAccept()`), không cho ứng dụng xóa thêm.
    - Lúc này chắc chắn $N$ ký tự cũ đã được xóa hoàn tất.
    - Clak lập tức gọi `doCommitString(pending_commit_string_)`.

---

## 3. Bộ đệm và Xử lý Lại Phím (Buffer & Replay)

Khi một chu trình Uinput đang diễn ra (`is_deleting_ = true`):

- Nếu người dùng gõ phím tiếp theo trước khi phím chốt quay về, phím đó sẽ được gom vào danh sách `buffered_keys_` và tạm chặn gửi ra ngoài để tránh đảo lộn thứ tự.
- Khi phím chốt quay về và commit xong ký tự tiếng Việt, hàm `replayBufferedKeys()` sẽ duyệt lại danh sách phím đệm:
    - Nếu phím kế tiếp cần xử lý tiếp qua engine: gọi `handleKey`.
    - Nếu là các ký tự gõ thô (chữ thường, dấu cách): Clak gộp tất cả ký tự thô liên tiếp vào một chuỗi `batch_commit` duy nhất rồi gửi qua `doCommitString`, giảm thiểu tối đa số lần gọi IPC qua Wayland.

---

## 4. Bộ Đếm An toàn (Safety Timer)

Nhằm đề phòng trường hợp ứng dụng đích bị crash hoặc compositor nuốt mất phím chốt khiến Clak bị kẹt vĩnh viễn ở trạng thái xóa:

- Mỗi khi phát phím Uinput, Clak hẹn giờ `safety_timer_` với thời gian 50ms (hoặc 100ms trên thanh địa chỉ).
- Nếu hết thời gian mà chưa nhận đủ số phím BackSpace mong muốn, timer sẽ tự động kích hoạt: giải phóng cờ `is_deleting_`, commit phần ký tự đang chờ, và giải phóng bộ đệm phím.
