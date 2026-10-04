# Đặc tả giao diện lập trình Clak (API Reference)

Tài liệu chi tiết về toàn bộ giao diện C-FFI ngoại vi của Clak, bao gồm danh mục hàm, cấu trúc dữ liệu, hằng số phân loại và quy tắc quản lý bộ nhớ.

---

## 1. Tổng quan giao diện C-FFI

Lõi xử lý tiếng Việt của Clak được viết bằng Rust (`engine/`) và xuất khẩu qua chuẩn giao tiếp nhị phân C (C-ABI / `extern "C"`). Tầng C++ của Fcitx5 kết nối trực tiếp với thư viện này mà không phát sinh overhead chuyển đổi dữ liệu.

Các nhóm API chính:

- **IME Context API**: Quản lý phiên gõ phím, xử lý văn bản ngữ cảnh xung quanh và phát sinh hành động thay thế
- **Core Transform API**: Biến đổi chuỗi ký tự thô thành tiếng Việt theo phương pháp Telex, VNI, VIQR
- **Charset API**: Chuyển đổi qua lại giữa các bảng mã tiếng Việt cũ và mới
- **Config API**: Tải cấu hình, kiểm tra phím tắt và danh sách ứng dụng loại trừ

---

## 2. Giao diện ngữ cảnh nhập liệu (IME Context API)

Tập trung tại [engine/src/ime.rs](../engine/src/ime.rs).

### Danh mục hàm

| Tên hàm                     | Kiểu trả về        | Tham số                                                                 | Mô tả chức năng                                               |
| --------------------------- | ------------------ | ----------------------------------------------------------------------- | ------------------------------------------------------------- |
| `clak_context_new`          | `*mut ClakContext` | `method: i32`                                                           | Khởi tạo ngữ cảnh nhập liệu mới với phương pháp gõ chỉ định   |
| `clak_context_free`         | `void`             | `ctx: *mut ClakContext`                                                 | Giải phóng bộ nhớ ngữ cảnh nhập liệu                          |
| `clak_context_reset`        | `void`             | `ctx: *mut ClakContext`                                                 | Xoá bộ đệm gõ thô và đưa trạng thái về ban đầu                |
| `clak_context_apply_config` | `void`             | `ctx: *mut ClakContext, cfg: *const ClakConfig`                         | Áp dụng toàn bộ thiết lập cấu hình vào ngữ cảnh               |
| `clak_process_key`          | `ImeAction`        | `ctx, key_sym, key_str, has_ctrl_alt, surrounding_text, cursor, anchor` | Xử lý một sự kiện phím nhấn và sinh ra hành động cần thực thi |

### Cấu trúc dữ liệu `ImeAction`

```c
typedef struct {
    int32_t action_type;
    size_t delete_count;
    const char* commit_str;
} ImeAction;
```

### Danh mục loại hành động (`action_type`)

| Mã  | Tên hằng số                  | Mô tả hành vi                                            | Kênh xử lý đề xuất                  |
| --- | ---------------------------- | -------------------------------------------------------- | ----------------------------------- |
| `0` | `ACTION_FORWARD`             | Không biến đổi, chuyển tiếp phím nguyên bản cho ứng dụng | Forward trực tiếp                   |
| `1` | `ACTION_COMMIT`              | Chèn chuỗi ký tự mới mà không cần lùi xóa                | Fcitx5 `commitString`               |
| `2` | `ACTION_REPLACE_SURROUNDING` | Thay thế dựa trên văn bản ngữ cảnh xung quanh            | `deleteSurroundingText` hoặc Uinput |
| `3` | `ACTION_ADDRESS_BAR_FIX`     | Xóa bù ký tự gợi ý tự động trên thanh địa chỉ            | Uinput (+1 Backspace autofill)      |
| `4` | `ACTION_REPLACE`             | Xóa lùi `delete_count` ký tự UTF-8 và chèn `commit_str`  | Uinput Sentinel hoặc Surrounding    |

### Bảng mã phương pháp gõ (`method`)

| Giá trị | Phương pháp | Mô tả                                                        |
| ------- | ----------- | ------------------------------------------------------------ |
| `0`     | Telex       | Gõ lặp nguyên âm và phụ âm quy ước (`aa` -> `â`, `s` -> sắc) |
| `1`     | VNI         | Dùng các phím số từ 1 đến 9 để bỏ dấu                        |
| `2`     | VIQR        | Dùng các ký tự quy ước như `^`, `(`, `'`                     |
| `3`     | Teip VNI    | Biến thể kết hợp tối ưu cho bàn phím số                      |

---

## 3. Giao diện biến đổi chuỗi (Core Transform API)

Tập trung tại [engine/src/lib.rs](../engine/src/lib.rs).

### Danh mục hàm

| Tên hàm                      | Kiểu trả về     | Tham số                                       | Mô tả chức năng                                                             |
| ---------------------------- | --------------- | --------------------------------------------- | --------------------------------------------------------------------------- |
| `clak_core_new`              | `*mut ClakCore` | `method: i32`                                 | Tạo đối tượng máy biến đổi ngôn ngữ thuần túy                               |
| `clak_core_free`             | `void`          | `engine: *mut ClakCore`                       | Giải phóng bộ nhớ của đối tượng máy biến đổi                                |
| `clak_core_set_method`       | `void`          | `engine: *mut ClakCore, method: i32`          | Thay đổi phương pháp gõ (Telex, VNI, VIQR)                                  |
| `clak_core_set_tone_style`   | `void`          | `engine: *mut ClakCore, modern: i32`          | Đặt chuẩn dấu thanh: kiểu mới (`1`) hoặc kiểu cũ (`0`)                      |
| `clak_core_set_free_marking` | `void`          | `engine: *mut ClakCore, enabled: i32`         | Bật hoặc tắt tính năng đặt dấu tự do                                        |
| `clak_core_set_short_w`      | `void`          | `engine: *mut ClakCore, enabled: i32`         | Bật hoặc tắt gõ tắt `w` thành `ư`                                           |
| `clak_core_set_auto_restore` | `void`          | `engine: *mut ClakCore, enabled: i32`         | Bật hoặc tắt tự động phục hồi từ tiếng Anh                                  |
| `clak_core_set_dict`         | `void`          | `engine: *mut ClakCore, enabled: i32`         | Bật hoặc tắt kiểm tra từ điển tiếng Việt                                    |
| `clak_core_add_word`         | `void`          | `engine: *mut ClakCore, word: *const char`    | Thêm một từ vào từ điển người dùng tuỳ biến                                 |
| `clak_core_clear_words`      | `void`          | `engine: *mut ClakCore`                       | Xoá toàn bộ từ điển người dùng                                              |
| `clak_core_dict_words`       | `*mut char`     | Không có                                      | Lấy toàn bộ danh sách từ điển nhúng (phân tách bởi dòng mới)                |
| `clak_core_transform`        | `*mut char`     | `engine: *const ClakCore, input: *const char` | Biến đổi chuỗi thô thành từ tiếng Việt hoàn chỉnh                           |
| `clak_core_is_valid`         | `i32`           | `s: *const char`                              | Kiểm tra chuỗi có tuân thủ cấu trúc phụ âm - nguyên âm tiếng Việt (`1`/`0`) |

---

## 4. Giao diện bảng mã và giải mã (Charset API)

Tập trung tại [engine/src/charset.rs](../engine/src/charset.rs).

### Danh mục hàm

| Tên hàm                    | Kiểu trả về | Tham số                                                 | Mô tả chức năng                                        |
| -------------------------- | ----------- | ------------------------------------------------------- | ------------------------------------------------------ |
| `clak_charset_encode`      | `*mut u8`   | `input: *const char, charset: i32, out_len: *mut usize` | Mã hoá chuỗi UTF-8 sang mảng byte của bảng mã chỉ định |
| `clak_charset_decode`      | `*mut char` | `input: *const u8, len: usize, charset: i32`            | Giải mã mảng byte thành chuỗi Unicode UTF-8 dựng sẵn   |
| `clak_charset_remove_tone` | `*mut char` | `input: *const char`                                    | Loại bỏ toàn bộ dấu thanh tiếng Việt khỏi chuỗi        |
| `clak_charset_free_buf`    | `void`      | `ptr: *mut u8`                                          | Giải phóng mảng byte do `clak_charset_encode` cấp phát |

### Bảng mã ký tự (`charset`)

| Mã  | Định danh   | Bảng mã tương ứng                  |
| --- | ----------- | ---------------------------------- |
| `0` | `Unicode`   | Chuẩn Unicode dựng sẵn (NFC)       |
| `1` | `TCVN3`     | Chuẩn TCVN3 (ABC, font .VnTime)    |
| `2` | `VNIWin`    | Chuẩn VNI Windows (font VNI-Times) |
| `3` | `WinCP1258` | Chuẩn Windows-1258 tổ hợp          |
| `4` | `VIQR`      | Chuẩn văn bản thuần VIQR           |

---

## 5. Giao diện đọc cấu hình (Config API)

Tập trung tại [engine/src/config.rs](../engine/src/config.rs).

### Danh mục hàm

| Tên hàm                           | Kiểu trả về       | Tham số                                         | Mô tả chức năng                                                |
| --------------------------------- | ----------------- | ----------------------------------------------- | -------------------------------------------------------------- |
| `clak_config_path`                | `*mut char`       | Không có                                        | Lấy đường dẫn file cấu hình TOML trên hệ thống                 |
| `clak_config_load`                | `*mut ClakConfig` | Không có                                        | Đọc và nạp cấu hình từ đĩa                                     |
| `clak_config_default`             | `*mut ClakConfig` | Không có                                        | Khởi tạo cấu hình với các giá trị mặc định                     |
| `clak_config_free`                | `void`            | `cfg: *mut ClakConfig`                          | Giải phóng đối tượng cấu hình                                  |
| `clak_config_is_app_excluded`     | `bool`            | `cfg: *const ClakConfig, app_name: *const char` | Kiểm tra ứng dụng có nằm trong danh sách loại trừ hay không    |
| `clak_config_get_remember_state`  | `bool`            | `cfg: *const ClakConfig`                        | Kiểm tra tuỳ chọn nhớ trạng thái gõ theo từng ứng dụng         |
| `clak_config_get_uinput_ack`      | `bool`            | `cfg: *const ClakConfig`                        | Kiểm tra cờ bật xác nhận loopback uinput                       |
| `clak_config_get_debug_log`       | `bool`            | `cfg: *const ClakConfig`                        | Kiểm tra cờ ghi log chẩn đoán                                  |
| `clak_config_get_startup_mode`    | `i32`             | `cfg: *const ClakConfig`                        | Lấy chế độ khởi động (`0`: tiếng Việt, `1`: tiếng Anh)         |
| `clak_config_get_method`          | `i32`             | `cfg: *const ClakConfig`                        | Lấy phương pháp gõ mặc định (`0`: Telex, `1`: VNI...)          |
| `clak_config_get_toggle_shortcut` | `*mut char`       | `cfg: *const ClakConfig`                        | Lấy chuỗi phím tắt bật/tắt bộ gõ (ví dụ: `ctrl_shift`)         |
| `clak_config_get_switch_shortcut` | `*mut char`       | `cfg: *const ClakConfig`                        | Lấy chuỗi phím tắt chuyển phương pháp gõ (ví dụ: `ctrl_space`) |

---

## 6. Quy tắc quản lý bộ nhớ

Nhằm tránh rò rỉ bộ nhớ giữa ranh giới Rust và C++:

1. **Chuỗi trả về kiểu con trỏ (`*mut char`)**:
    - Mọi chuỗi ký tự trả về từ các hàm như `clak_core_transform`, `clak_charset_remove_tone`, `clak_config_path` đều được cấp phát bởi `CString::into_raw()`.
    - Bắt buộc phải được giải phóng bằng hàm `clak_free_string(ptr)` khi không còn sử dụng.
2. **Bộ đệm nhị phân (`*mut u8`)**:
    - Bộ đệm nhị phân trả về từ `clak_charset_encode` được cấp phát qua `libc::malloc`.
    - Bắt buộc phải được giải phóng bằng hàm `clak_charset_free_buf(ptr)`.
3. **Các cấu trúc đối tượng (`ClakContext`, `ClakCore`, `ClakConfig`)**:
    - Được đóng gói dưới dạng con trỏ thô thông qua `Box::into_raw()`.
    - Bắt buộc phải giải phóng bằng hàm destructor tương ứng (`clak_context_free`, `clak_core_free`, `clak_config_free`).
