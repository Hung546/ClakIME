# Module Nền tảng và Nhận diện Ứng dụng (Platform & Window Info)

Thư mục: [src/platform/](../src/platform/), [src/config/](../src/config/)

Các file chính: [hyprland.cpp](../src/platform/hyprland.cpp), [window_info.h](../src/platform/window_info.h), [modal_editor.cpp](../src/platform/modal_editor.cpp), [sites.cpp](../src/config/sites.cpp)

Module này thu thập ngữ cảnh của ứng dụng đang được người dùng tương tác để Clak tự động cấu hình hành vi thích hợp mà không cần người dùng phải bấm phím chuyển chế độ thủ công.

---

## 1. Giao tiếp Socket IPC với Hyprland

Trên Wayland, vì lý do bảo mật, ứng dụng thông thường không thể tự do truy vấn thông tin các cửa sổ khác. Tuy nhiên compositor Hyprland cung cấp giao tiếp UNIX domain socket tại:

`$XDG_RUNTIME_DIR/hypr/$HYPRLAND_INSTANCE_SIGNATURE/.socket.sock`

Trong file [hyprland.cpp](../src/platform/hyprland.cpp):

1. Clak mở kết nối Non-blocking Socket tới socket của Hyprland.
2. Gửi lệnh ngắn `j/activewindow`.
3. Dùng `poll` với timeout cực nhanh (15ms) để nhận JSON phản hồi chứa `class`, `title`, và `pid`.
4. **Bộ nhớ đệm (Cache 500ms)**: Kết quả truy vấn được lưu lại trong 500ms. Trong thời gian này, mọi phím gõ liên tiếp đều dùng lại kết quả cũ, giảm tải 100% việc tạo socket lặp đi lặp lại.

---

## 2. Nhận diện Domain Website và Nhóm Ứng dụng

File [sites.cpp](../src/config/sites.cpp) phân tích tiêu đề cửa sổ trình duyệt để trích xuất domain đang hoạt động:

- **Họ Gecko (isGeckoApp)**: Firefox, Zen Browser, Librewolf, Floorp, Waterfox...
- **Họ Chromium (isChromiumApp)**: Chrome, Chromium, Brave, Helium, Edge, Vivaldi, Thorium...
- **Họ Terminal (isTerminalApp)**: Kitty, Ghostty, Alacritty, Foot, Wezterm, GNOME Terminal...
- **Dịch vụ buộc dùng Uinput (isForceUinputSite)**: Google Docs, Google Sheets, Google Slides, TikTok, X (Twitter)...
- **Dịch vụ Meta (isMetaSite)**: Facebook, Messenger, Instagram, Threads, WhatsApp (dùng cơ chế SurroundingText tiêu chuẩn để tránh lỗi con trỏ văn bản của Draft.js).

---

## 3. Nhận diện Trình soạn thảo Modal (Modal Editors)

File [modal_editor.cpp](../src/platform/modal_editor.cpp):

Khi người dùng làm việc trong Neovim, Vim, Kakoune, hoặc Helix (chạy trong Terminal hoặc bản GUI như Neovide):

- Clak theo dõi trạng thái biên tập: `NORMAL`, `INSERT`, `COMMAND`.
- Khi ở chế độ `NORMAL`:
    - Các phím lệnh như `h`, `j`, `k`, `l`, `d`, `y`, `w` được truyền thẳng ra ngoài mà không bao giờ bị biến đổi thành ký tự có dấu.
    - Các phím chuyển chế độ như `i`, `a`, `o`, `c`, `s` chuyển trạng thái sang `INSERT` và reset engine để chuẩn bị gõ tiếng Việt mượt mà.
- Khi người dùng nhấn `Escape` hoặc `Ctrl+[`: Clak lập tức quay về trạng thái `NORMAL` và dọn sạch bộ đệm gõ.
