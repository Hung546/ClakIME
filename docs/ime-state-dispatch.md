# Module Điều hướng Trạng thái Gõ (IME State & Dispatch)

Thư mục: [src/ime/](./src/ime/)

File chính: [state.h](./src/ime/state.h), [state.cpp](./src/ime/state.cpp)

Đây là khối điều khiển trung tâm của Clak trên Fcitx5, quản lý toàn bộ vòng đời phím bấm, quyết định sử dụng kênh Wayland SurroundingText hay Uinput, và đảm bảo tính toàn vẹn (correctness) của văn bản hiển thị.

---

## 1. Cơ chế Quyết định Kênh Xóa (shouldUseUinput)

Trên Wayland, các ứng dụng có cách cài đặt giao thức `zwp_text_input_v3` rất khác nhau:

- **Chromium / Chrome / Brave**: Hỗ trợ `delete_surrounding_text` cực tốt, độ trễ < 1ms. Clak dùng kênh SurroundingText trực tiếp khi gõ bình thường ở cuối câu.
- **Chèn con trỏ giữa từ hoặc trước từ (isCursorNearWord)**: Khi phát hiện phía sau con trỏ còn ký tự trong trình duyệt (người dùng click chuột vào giữa câu để sửa hoặc chèn từ), Blink dễ bị lỗi lệch offset. Clak tự động chuyển nhánh sang **Uinput** để xóa và chèn chính xác.
- **Họ VSCode (Antigravity IDE, VSCode, Cursor, Windsurf, VSCodium)**: Terminal tích hợp bên trong chạy bằng `xterm.js`. Thành phần này hoàn toàn không hỗ trợ lệnh `delete_surrounding_text` của Wayland (gây lỗi `irỉ`, `ỉi`). Clak gom họ VSCode vào nhóm Terminal và chuyển sang **Uinput** với nhịp pacing 15ms/4ms.
- **Gecko (Firefox / Zen Browser)**: Lỗi xóa xung quanh kéo dài nhiều năm trên Wayland (DOM không cập nhật kịp thời, text bị stale). Clak bắt buộc điều hướng sang kênh **Uinput**.
- **Google Docs (Canvas Editor)**: Không dùng DOM HTML thông thường mà vẽ chữ lên HTML5 Canvas, surrounding text chỉ là 2 dấu cách giả lập (`  `). Clak bắt buộc điều hướng sang **Uinput**.
- **Terminal Native (Kitty, Ghostty, Alacritty, Foot)**: Terminal bảo vệ buffer PTY, không hỗ trợ xóa lùi ngữ cảnh Wayland. Clak dùng **Uinput**.
- **Address Bar (Thanh địa chỉ URL)**: Khi xuất hiện gợi ý tự động (autofill), con trỏ bị bôi đen hoặc nhảy về cuối. Clak dùng Uinput kèm thuật toán đếm bù 1 ký tự gợi ý.

```cpp
bool ClakState::shouldUseUinput(bool use_surrounding, uint32_t action_type, const fcitx::SurroundingText& surr) {
    std::string app = appKey();
    std::string site = activeSite();

    if (config::isTerminalApp(app)) return true;
    if (config::isGeckoApp(app)) return true;
    if (config::isMetaSite(site) || config::isMetaSite(app)) return false;
    if (action_type == CLAK_ACTION_ADDRESS_BAR_FIX) return true;
    if (config::isForceUinputSite(site)) return true;
    if (is_canvas_editor_ || is_rich_text_editor_) return true;
    if (isBrowser() && isCursorNearWord(surr)) return true;
    return !use_surrounding;
}
```

---

## 2. Tại sao Không Mặc định Uinput cho Tất cả Ứng dụng?

Có 5 lý do kỹ thuật cốt lõi khiến Clak ưu tiên SurroundingText và chỉ dùng Uinput ở nơi thực sự cần:

1. **Tốc độ phản hồi cực hạn (0.3ms so với 16ms)**:
    - SurroundingText chạy qua IPC Wayland trực tiếp trong bộ nhớ RAM, độ trễ chỉ 0.2ms đến 0.3ms, gõ tốc độ cao 150 WPM hoàn toàn không có cảm giác trễ
    - Uinput phải bắn phím qua kernel driver `/dev/uinput` -> compositor -> ứng dụng đích -> loopback lại fcitx5, bắt buộc cần nhịp pacing 15ms đến 20ms
2. **Bảo vệ DOM các trang Meta (Facebook, Messenger, Instagram)**:
    - Trình soạn thảo Draft.js / Lexical của Meta quản lý con trỏ DOM rất nhạy cảm. Phím Backspace vật lý từ Uinput sẽ phá vỡ cấu trúc khối, làm con trỏ nhảy ngược ra đầu dòng hoặc xóa mất cả đoạn văn
    - SurroundingText cho phép fcitx5 thay đổi chuỗi trực tiếp trong bộ đệm mà không phát sinh phím xóa vật lý
3. **Mắt thần đọc ngữ cảnh (Context Awareness)**:
    - SurroundingText cho Clak biết chính xác từ ngữ đứng trước và đứng sau con trỏ chuột, nhờ đó phục hồi dấu hoặc sửa từ cũ thông minh khi người dùng click chuột quay lại từ đã gõ
    - Uinput là thiết bị phát phím mù, không thể đọc ngược dữ liệu trên màn hình
4. **Blink nuốt phím cứng khi IME đang active**:
    - Trên Chromium, khi ô nhập liệu đang trong phiên gõ IME, Blink ưu tiên nhận văn bản từ giao thức IME. Phím xóa cứng từ Uinput dễ bị xem là xung đột và bị nuốt mất, khiến bộ gõ bị kẹt ở trạng thái chờ
5. **Quyền hạn `/dev/uinput` trên Linux**:
    - Nhiều bản phân phối Linux không phân quyền mặc định `/dev/uinput` cho user thông thường. SurroundingText chạy qua Wayland IPC nên hoạt động ngay lập tức mà không cần cấu hình udev rule hay quyền root

---

## 3. Kỹ thuật Phím Chốt (Sentinel Backspace)

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

## 4. Bộ đệm và Xử lý Lại Phím (Buffer & Replay)

Khi một chu trình Uinput đang diễn ra (`is_deleting_ = true`):

- Nếu người dùng gõ phím tiếp theo trước khi phím chốt quay về, phím đó sẽ được gom vào danh sách `buffered_keys_` và tạm chặn gửi ra ngoài để tránh đảo lộn thứ tự.
- Khi phím chốt quay về và commit xong ký tự tiếng Việt, hàm `replayBufferedKeys()` sẽ duyệt lại danh sách phím đệm:
    - Nếu phím kế tiếp cần xử lý tiếp qua engine: gọi `handleKey`.
    - Nếu là các ký tự gõ thô (chữ thường, dấu cách): Clak gộp tất cả ký tự thô liên tiếp vào một chuỗi `batch_commit` duy nhất rồi gửi qua `doCommitString`, giảm thiểu tối đa số lần gọi IPC qua Wayland.

---

## 5. Bộ Đếm An toàn (Safety Timer)

Nhằm đề phòng trường hợp ứng dụng đích bị crash hoặc compositor nuốt mất phím chốt khiến Clak bị kẹt vĩnh viễn ở trạng thái xóa:

- Mỗi khi phát phím Uinput, Clak hẹn giờ `safety_timer_` với thời gian 50ms (hoặc 100ms trên thanh địa chỉ).
- Nếu hết thời gian mà chưa nhận đủ số phím BackSpace mong muốn, timer sẽ tự động kích hoạt: giải phóng cờ `is_deleting_`, commit phần ký tự đang chờ, và giải phóng bộ đệm phím.
