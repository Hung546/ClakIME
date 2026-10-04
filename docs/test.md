# Hệ thống kiểm thử tự động Clak

Tài liệu chi tiết về kiến trúc kiểm thử tự động, danh mục test case và kịch bản phòng ngừa lỗi (regression) trong Clak.

---

## 1. Tổng quan kiến trúc kiểm thử

Hệ thống kiểm thử của Clak được phân tách thành 3 tầng độc lập, đảm bảo kiểm chứng từ thuật toán ngôn ngữ thuần túy đến máy trạng thái bất đồng bộ và mô phỏng tải hệ thống thực:

| Tầng kiểm thử | Vị trí mã nguồn | Công nghệ | Mục đích | Số lượng test |
| --- | --- | --- | --- | --- |
| **Rust engine unit và fuzzing** | `engine/src/`, `engine/tests/` | `cargo test`, `proptest` | Kiểm thử bảng mã, quy tắc ghép âm Telex/VNI, từ điển, tối thiểu hoá diff và fuzzing bất biến | 114 tests |
| **C++ state machine và regression** | `src/tests/` | GoogleTest, Fcitx5 mock | Kiểm thử máy trạng thái C++, bẫy sentinel, race condition, rò rỉ bộ đệm và tương thích compositor | 17 tests |
| **E2E desktop scripts** | `scripts/tests/` | Bash, `just` | Mô phỏng phím gõ thực tế trên browser, đo latency và kiểm tra tải | 4 kịch bản |

---

## 2. Kiểm thử C++ state machine và regression (`src/tests/`)

Tầng này kế thừa trực tiếp từ các class thật của Fcitx5 (`fcitx::Instance`, `fcitx::InputContext`, `fcitx::EventLoop`), mock phần cứng uinput và cửa sổ desktop để kiểm thử độc lập mà không bị ảnh hưởng bởi môi trường ngoài.

### Danh mục test case máy trạng thái C++ (`test_clak_state.cpp`)

| Tên test case | Mục đích kiểm tra | Kịch bản bảo vệ |
| --- | --- | --- |
| `GivenGeckoApp_AlwaysRoutesToUinput` | Ứng dụng nền Gecko (Firefox, Zen Browser) luôn ép đi qua kênh uinput | Ngăn chặn lỗi nuốt phím và xung đột bộ đệm nội bộ của Gecko khi dùng SurroundingText |
| `GivenStaleSurroundingText3TimesInRow_FallsBackToUinput` | Cơ chế tự động phát hiện surrounding text bị đơ (stale 3 lần liên tiếp) | Tự động hạ cấp sang uinput để cứu phiên gõ, không làm kẹt chữ người dùng |
| `GivenSentinelNeverArrives_SafetyTimerFires_RecoversAndCommits` | Sentinel Backspace bị mất do compositor hoặc app nuốt | Safety timer 50ms tự kích hoạt, cam kết text đang chờ và giải phóng trạng thái kẹt |
| `GivenKeysBufferedDuringDelete_ReplaysInOriginalOrder` | Người dùng gõ cực nhanh (>120 WPM) trong lúc uinput đang xoá | Các phím bấm sau được đưa vào hàng đợi và phát lại đúng thứ tự sau khi xoá xong |
| `GivenChromiumNormalPage_UsesSurroundingText` | Trang web thông thường trên Chromium/Brave | Ưu tiên dùng SurroundingText trực tiếp để đạt độ trễ thấp nhất (zero perceptible latency) |
| `GivenGnomeFallbackApp_DetectsGuiEditorWithoutCompositorPid` | Trình soạn thảo GUI (Neovide, GVim) trên môi trường không có socket compositor (GNOME) | Tự động nhận diện modal editor qua tên chương trình fallback từ Fcitx5 |
| `GivenGnomeFallbackApp_PopulatesTerminalClass` | Terminal trên GNOME (gnome-terminal-server) khi không có IPC Hyprland | Điền class từ fallback để quét tiến trình con trong /proc |

### Danh mục test case phòng ngừa lỗi (`test_regression.cpp`)

| Tên test case | Lỗi thực tế ngăn chặn | Giải pháp kỹ thuật |
| --- | --- | --- |
| `test_regression_address_bar_dd_to_d` | Gõ `dd` trên thanh địa chỉ Chromium bị mất ký tự hoặc ra `d` | Nhận diện Omnibox inline autocomplete, phát 3 Backspace (1 ký tự + 1 xoá gợi ý + 1 sentinel) |
| `test_regression_hyprland_stale_selection_during_retoning` | Đổi dấu từ ở giữa (như `dựng`) trên Hyprland/Niri bị mất chữ cái đầu `d` | `isAutofillCertain` chỉ kích hoạt khi vùng chọn kéo dài đến cuối chuỗi (`sel_end == text.length()`) |
| `test_regression_docs_cascading_chars` | Lặp ký tự kiểu thác đổ liên tục trên canvas Google Docs | Nhận diện Google Docs qua active window title và ép chuyển toàn bộ thao tác sang uinput |
| `test_regression_laf_sentinel_timeout` | Sentinel bị mất trong lúc gõ văn bản thông thường | Safety timer phục hồi nhanh trong 50ms, cam kết đúng ký tự mà không gây lag |
| `test_regression_rapid_selection_deletion` | Bôi đen nhanh một khối văn bản rồi gõ đè | Tách riêng timeout 250ms cho kịch bản bôi đen, ngăn safety timer 50ms ngắt nhầm ở 100ms |
| `test_regression_safety_timer_self_reset_crash` | Crash use-after-free trên sd-event loop khi timeout xảy ra | Callback timer không tự gọi `reset()` lên chính nó trong lúc đang dispatch sự kiện |
| `test_regression_gecko_stale_surrounding` | Trình duyệt Gecko gửi surrounding text không hợp lệ | Đảm bảo không bao giờ gọi hàm `deleteSurroundingText` trên mọi biến thể của Gecko |
| `test_regression_twitter_draftjs_dd` | Soạn thảo bài viết trên Twitter/X dùng framework Draft.js | Phát hiện 3 lần surrounding text cũ liên tiếp và tự động định tuyến sang uinput |
| `test_regression_terminal_forward_key_dup` | Gõ phím trên terminal (Kitty, Alacritty, Ghostty) bị nhân đôi ký tự | Kiểm tra cấu hình và ép chuyển kênh uinput cho các cửa sổ terminal |
| `test_regression_adaptive_wait_scales_and_decays` | Hệ thống bị giật lag khiến độ trễ loopback tăng cao | Tự động nâng thời gian chờ an toàn (+10ms/bước) khi trễ (>35ms) và decay (-10ms) khi ổn định |

---

## 3. Kiểm thử lõi Rust engine (`engine/`)

### 1. Kiểm tra bảng mã và biến đổi ký tự (`charset.rs`, `tables.rs`)

- Chuyển đổi qua lại giữa Unicode dựng sẵn (NFC) và Unicode tổ hợp (NFD)
- Bảo toàn tính toàn vẹn cho các bảng mã cũ: VNI Windows, TCVN3 (ABC), Windows-1258, VIQR
- Đảm bảo các ký tự tiếng Anh thông thường đi qua không bị biến dạng

### 2. Thuật toán ghép âm và bỏ dấu (`engine.rs`)

- **Telex cơ bản**: `aa` -> `â`, `aw` -> `ă`, `ee` -> `ê`, `oo` -> `ô`, `ow` -> `ơ`, `uw` -> `ư`, `dd` -> `đ`
- **Quy tắc dấu thanh**: Sắc (`s`), Huyền (`f`), Hỏi (`r`), Ngã (`x`), Nặng (`j`)
- **Dấu mới vs Dấu cũ**: Hỗ trợ chuyển đổi giữa chuẩn hiện đại (`hoá`, `oà`, `uý`) và chuẩn truyền thống (`hóa`, `òa`, `úy`)
- **Phím huỷ dấu**: Phím `z` phục hồi trạng thái nguyên âm gốc (`toán` + `z` -> `toan`)
- **Đảo nguyên âm**: Đảo dấu thông minh khi gõ tiếp (`uo` + `w` -> `ươ`)
- **VNI & VIQR**: Hỗ trợ đầy đủ phím số (1-9) và ký hiệu VIQR

### 3. Tùy chọn nhập liệu nâng cao (`engine/tests/test_config_toggles.rs`)

| Tên test case | Tính năng kiểm tra | Quy tắc hoạt động |
| --- | --- | --- |
| `test_toggle_auto_capitalize` | Tự động viết hoa đầu câu | Viết hoa sau `. `, `? `, `! `; huỷ khi gặp Backspace/Enter; không viết hoa khi thiếu khoảng trắng |
| `test_toggle_double_space_period` | Nhấn 2 lần Space ra dấu chấm | Thay thế khoảng trắng liền trước bằng `. ` khi bật tính năng |
| `test_toggle_macro_expansion` | Bảng gõ tắt (Macro) | Hỗ trợ khớp hoa thường (thường, VIẾT HOA, Viết Hoa Đầu Từ) và kích hoạt bằng Space/Enter/Tab |
| `test_toggle_auto_restore` | Tự phục hồi từ tiếng Anh | Tự động trả về chữ gốc khi từ gõ vào không đúng quy tắc chính tả tiếng Việt |
| `test_toggle_modern_tone_telex` | Đặt dấu kiểu mới | Đảm bảo chuyển đổi chính xác vị trí dấu thanh theo tùy chọn người dùng |

### 4. Bất biến và tối thiểu hoá diff (`test_diff_minimal.rs`, `test_proptest.rs`)

- `test_diff_minimality_property`: Đảm bảo số lượng ký tự Backspace phát ra luôn là số lượng nhỏ nhất có thể
- `prop_telex_transform_never_panics`: Fuzzing chuỗi ký tự ngẫu nhiên, chứng minh bộ gõ không bao giờ panic
- `prop_compare_and_split_always_reconstructs`: Bất biến toán học: `base - del + added == target` luôn đúng với mọi từ

---

## 4. Hướng dẫn chạy kiểm thử

Mọi tác vụ kiểm thử đều được cấu hình trong `justfile`:

```bash
# Chạy toàn bộ unit test của lõi Rust engine (114 tests)
just test-unit

# Build và chạy toàn bộ C++ state machine & regression tests (15 tests)
just test-cpp

# Chạy toàn bộ pipeline kiểm tra trước khi push (fmt, clippy, unit, cpp, build)
just check

# Chạy kiểm thử tốc độ gõ phím mô phỏng
just test-speed 20

# Chạy kiểm thử thanh địa chỉ Chromium
just test-chromium 15
```
