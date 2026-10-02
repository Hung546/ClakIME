# Module Lõi Xử lý Ngôn ngữ (Engine)

Thư mục: [engine/](./engine/)

Lõi xử lý tiếng Việt của Clak được viết hoàn toàn bằng Rust để đảm bảo an toàn bộ nhớ và tốc độ xử lý nano-giây. Module này chịu trách nhiệm biến chuỗi phím người dùng gõ thành từ tiếng Việt có nghĩa theo quy tắc Telex hoặc VNI.

---

## 1. Kiến trúc Module Rust

- [lib.rs](./engine/src/lib.rs): Cung cấp giao diện C-FFI để tầng C++ của Fcitx5 gọi vào.
- [engine.rs](./engine/src/engine.rs): Máy trạng thái máy gõ (Telex, VNI, VIQR), logic tích hợp ký tự, xóa lùi, và khôi phục từ nguyên bản.
- [spelling.rs](./engine/src/spelling.rs): Quy tắc chính tả tiếng Việt chuẩn, nhận diện phụ âm đầu, nguyên âm đơn/đôi/ba, phụ âm cuối, và từ điển tiếng Việt rút gọn.
- [charset.rs](./engine/src/charset.rs): Bảng mã biến đổi giữa Unicode dựng sẵn, Unicode tổ hợp, VNI-Windows, và TCVN3.
- [tables.rs](./engine/src/tables.rs): Bảng tra cứu nguyên âm và dấu thanh tiếng Việt nhanh.
- [ime.rs](./engine/src/ime.rs): Trình mô phỏng tích hợp trực tiếp kiểm tra tính hợp lệ của ngữ cảnh xung quanh văn bản (surrounding text).

---

## 2. Giao diện FFI (C-Interface)

Tầng C++ giao tiếp với Rust qua các hàm ngoại vi trong [lib.rs](./engine/src/lib.rs):

```c
ClakContext* clak_context_new(uint32_t method);
void clak_context_free(ClakContext* ctx);
void clak_context_reset(ClakContext* ctx);

ClakAction clak_process_key(
    ClakContext* ctx,
    uint32_t key_sym,
    const char* key_str,
    bool has_ctrl_alt,
    const char* surrounding_text,
    size_t cursor_pos,
    size_t anchor_pos
);
```

### Cấu trúc ClakAction

Kết quả trả về cho mỗi phím nhấn là một struct `ClakAction`:

- `action_type`:
    - `0 (CLAK_ACTION_FORWARD)`: Phím không nằm trong quy tắc gõ, truyền thẳng cho ứng dụng.
    - `1 (CLAK_ACTION_COMMIT)`: Chèn một chuỗi ký tự mới mà không cần xóa trước con trỏ.
    - `2 (CLAK_ACTION_REPLACE)`: Xóa `delete_count` ký tự trước con trỏ và chèn `commit_str`.
    - `3 (CLAK_ACTION_REPLACE_SURROUNDING)`: Thay thế dựa trên văn bản ngữ cảnh xung quanh.
    - `4 (CLAK_ACTION_ADDRESS_BAR_FIX)`: Tín hiệu đặc biệt yêu cầu xử lý riêng cho ô địa chỉ URL.
- `delete_count`: Số ký tự UTF-8 cần lùi xóa.
- `commit_str`: Chuỗi ký tự tiếng Việt cần chèn vào.

---

## 3. Các Tính năng Đặc thù của Engine

1. **Khôi phục từ tiếng Anh (Auto Restore)**:
    - Khi gõ một từ tiếng Anh (ví dụ: `windows`, `free`, `agree`), engine sẽ không để dấu tiếng Việt làm hỏng từ mà tự động hoàn trả lại từ nguyên bản.
2. **Quy tắc gõ `dd` thông minh**:
    - Khi gõ `d` rồi xóa, rồi gõ lại `d` trong thanh địa chỉ hoặc trang web, engine xử lý chuẩn xác không để sót ký tự thừa `dđ`.
3. **Đặt dấu kiểu mới và truyền thống**:
    - Tự động nhận diện cấu trúc âm tiết để đặt dấu thanh chuẩn (ví dụ: `hoà` hoặc `hòa`, `thuỷ` hoặc `thủy`).
4. **Hỗ trợ Surrounding Text thực thời**:
    - Khi chuột di chuyển hoặc người dùng click đổi vị trí con trỏ, engine đọc `surrounding_text` để dựng lại ngữ cảnh từ trước đó, cho phép bỏ thêm dấu vào từ đã gõ sẵn.
