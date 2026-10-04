# Tài liệu Kỹ thuật Bộ gõ Tiếng Việt Clak

> [!IMPORTANT]
> **Tài liệu này được tạo ra bởi AI**

Clak là bộ gõ tiếng Việt hiệu năng cao dành cho Linux / Wayland, được xây dựng như một plugin (addon) cho hệ thống Fcitx5 kết hợp cùng bộ máy xử lý Telex/VNI viết bằng Rust.

Bộ gõ giải quyết dứt điểm các bài toán cố hữu của Wayland như: gõ trên thanh địa chỉ trình duyệt bị nuốt ký tự hoặc nhân đôi `đ`, gõ trên các ứng dụng nền Gecko (Zen Browser, Firefox) bị xung đột bộ đệm, giật lag trên Google Docs, cũng như nhận diện thông minh chế độ Normal/Insert trong các trình soạn thảo modal như Neovim và Helix.

---

## 1. Cấu trúc Thư mục

```text
input-method/
├── CMakeLists.txt              # Cấu hình build C++ addon cho Fcitx5
├── justfile                    # Phím tắt tác vụ (build, deploy, test)
├── engine/                     # Lõi xử lý tiếng Việt bằng Rust
│   ├── Cargo.toml
│   ├── tests/                  # Bộ kiểm thử tích hợp và bất biến (proptest)
│   └── src/
│       ├── lib.rs              # C-FFI exports (clak_context_new, clak_process_key...)
│       ├── engine.rs           # Máy trạng thái Telex/VNI
│       ├── spelling.rs         # Quy tắc ghép âm tiếng Việt và từ điển
│       ├── charset.rs          # Bảng mã (Unicode, VNI, TCVN3...)
│       └── ime.rs              # Mô phỏng tích hợp xung quanh văn bản
├── src/                        # Tầng kết nối Fcitx5 bằng C++
│   ├── engine.h / .cpp         # Điểm khởi động Fcitx5 Addon
│   ├── ime/
│   │   ├── state.h / .cpp      # Máy trạng thái xử lý phím và điều hướng Surrounding/Uinput
│   ├── uinput/
│   │   ├── uinput.h / .cpp     # Phát phím phần cứng ảo qua /dev/uinput Linux
│   ├── platform/
│   │   ├── hyprland.cpp        # Giao tiếp IPC socket với compositor Hyprland
│   │   ├── modal_editor.cpp    # Nhận diện Vim/Neovim/Helix và tự động chuyển chế độ
│   │   └── window_info.h       # Cấu trúc thông tin cửa sổ đang active
│   ├── config/
│   │   ├── config.h            # Các hằng số thời gian và ngưỡng an toàn
│   │   └── sites.h / .cpp      # Nhận diện nhóm ứng dụng và domain website
│   ├── utils/
│   │   ├── log.h / .cpp        # Ghi log gỡ lỗi kèm xoay vòng dung lượng
│   │   └── text_utils.h / .cpp # Xử lý chuỗi UTF-8, đếm ký tự, tách từ
│   └── tests/                  # Bộ kiểm thử C++ State Machine và Regression
├── scripts/
│   └── tests/                  # Bộ script kiểm thử tự động và đo latency
└── docs/                       # Tài liệu chi tiết từng module
```

---

## 2. Luồng Xử lý Dữ liệu Tổng thể

Khi người dùng nhấn một phím:

1. **Fcitx5 Event Loop**: Bắt sự kiện trong [state.cpp](./src/ime/state.cpp) `keyEvent`.
2. **Kiểm tra Modal Editor**: Nếu đang ở trong Vim/Neovim ở chế độ NORMAL, phím được chuyển thẳng (forward) không qua gõ dấu.
3. **Rust Engine FFI**: Gọi [clak_process_key](./engine/src/lib.rs) cùng văn bản ngữ cảnh xung quanh (surrounding text).
4. **Phân nhánh thực thi Action**:
    - `FORWARD`: Không biến đổi, chuyển tiếp phím gốc.
    - `COMMIT`: Nhận diện từ hoàn chỉnh hoặc ký tự đặc biệt, chèn chuỗi ký tự qua Fcitx5.
    - `REPLACE`: Cần xóa $N$ ký tự trước con trỏ và thay thế bằng âm tiết tiếng Việt mới.
5. **Kênh Xóa Văn bản (Dispatching)**:
    - **Kênh SurroundingText**: Áp dụng cho các ô nhập liệu tiêu chuẩn trên Chromium, Brave, mạng xã hội Facebook/Messenger qua `deleteSurroundingText`.
    - **Kênh Uinput Pacing**: Áp dụng cho Gecko (Zen Browser, Firefox), Google Docs canvas, và thanh địa chỉ (Omnibox) đang hiện gợi ý tìm kiếm.

---

## 3. Bản đồ Tài liệu Chi tiết

Mỗi module được giải thích cặn kẽ trong các tài liệu sau:

- [Lõi Xử lý Ngôn ngữ Rust (Engine)](./docs/engine.md): Cơ chế Telex, quy tắc đặt dấu, kiểm tra ngữ pháp tiếng Việt và giao tiếp C-FFI.
- [Quản lý Trạng thái và Điều hướng Phím (IME State)](./docs/ime-state-dispatch.md): Thuật toán chọn kênh Surrounding vs Uinput, cơ chế phím chốt Sentinel Backspace, bộ đệm phím (Buffer & Replay), và bộ đếm an toàn Safety Timer.
- [Môi trường Cửa sổ và Nhận diện Ứng dụng (Platform)](./docs/platform-window.md): Giao thức IPC Unix Socket với Hyprland, nhận diện domain web, và chuyển đổi trạng thái Vim.
- [Bộ phát Phím ảo và Nhịp thời gian (Uinput Pacing)](./docs/uinput-pacing.md): Trình điều khiển `/dev/uinput`, kỹ thuật pacing với `post_delay` và `gap_ms` để trình duyệt không bị nuốt phím.
- [Đo đạc và Tối ưu Độ trễ (Benchmark & Latency)](./docs/benchmark-latency.md): Phương pháp đo latency từ lúc nhận keydown đến khi commit, bảng số liệu p50/p95/p99 của 5 nhóm ứng dụng.
- [Hệ thống Kiểm thử Tự động (Automated Tests)](./docs/test.md): Danh mục kiểm thử Rust engine, C++ state machine, kịch bản phòng ngừa lỗi (regression corpus) và hướng dẫn chạy test.
