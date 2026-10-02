# Module Bộ phát Phím ảo và Nhịp thời gian (Uinput & Pacing)

Thư mục: [src/uinput/](./src/uinput/)

File chính: [uinput.h](./src/uinput/uinput.h), [uinput.cpp](./src/uinput/uinput.cpp)

Module Uinput cung cấp khả năng phát các sự kiện phím mức nhân Linux (kernel input event) trực tiếp thông qua thiết bị `/dev/uinput`. Đây là cứu cánh quan trọng nhất để vượt qua các hạn chế và lỗi của giao thức Wayland text-input trên nhiều ứng dụng phức tạp.

---

## 1. Khởi tạo Thiết bị Phần cứng Ảo

Trong `init_direct_uinput()`:

- Mở thiết bị `/dev/uinput`.
- Đăng ký các mã phím phần cứng cần thiết: `KEY_BACKSPACE`, `KEY_LEFTSHIFT`, `KEY_LEFT`, `KEY_DELETE`.
- Thiết lập thông số thiết bị ảo mang tên `Clak-Uinput`.
- Tạo thiết bị thông qua `ioctl(UI_DEV_CREATE)`.

---

## 2. Kỹ thuật Điều tiết Nhịp (Pacing)

Khi phát liên tiếp nhiều phím BackSpace qua Uinput, nếu gửi ồ ạt trong cùng một micro-giây, luồng xử lý giao diện của trình duyệt (layout thread) hoặc Terminal buffer sẽ không kịp cập nhật, dẫn đến hiện tượng trượt phím, sót ký tự hoặc đảo lộn vị trí con trỏ.

Clak giải quyết vấn đề này bằng mô hình Pacing bất đồng bộ trên một worker thread tách biệt (`std::thread(...).detach()`):

```cpp
bool UinputTool::send_backspace_direct(size_t count, uint32_t post_delay_ms, uint32_t pre_delay_ms, uint32_t gap_ms) {
    std::thread([this, count, post_delay_ms, pre_delay_ms, gap_ms]() {
        std::lock_guard<std::mutex> lock(uinput_mutex_);

        // 1. Phát các phím xóa thật
        for (size_t i = 0; i < count - 1; ++i) {
            emit_bs();
            if (gap_ms > 0 && i + 1 < count - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(gap_ms));
            }
        }

        // 2. Chờ ứng dụng xóa xong trước khi phát phím chốt
        if (post_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(post_delay_ms));
        }

        // 3. Phát phím chốt (sentinel)
        emit_bs();
    }).detach();

    return true;
}
```

### Các thông số Pacing

1. `gap_ms` (Khoảng cách giữa các phím xóa thật):
    - Mặc định: `2ms`.
    - Giúp ứng dụng nhận diện và xóa từng ký tự một cách ổn định theo chu kỳ đồng hồ.
2. `post_delay_ms` (Khoảng chờ trước phím chốt):
   - Mặc định cho ứng dụng thông thường (Gecko, Docs, Terminal): `2ms`.
   - Riêng trường hợp Address Bar có gợi ý tự động (autofill popup): `20ms` (`kAddressBarPostDelayMs`). Mức delay 20ms này là bắt buộc để trình duyệt (Chromium/Gecko) có đủ thời gian đóng popup gợi ý và tính toán lại con trỏ, ngăn ngừa triệt để lỗi nuốt phím và lỗi nhân đôi `dd -> dđ`.
3. Không làm block Event Loop của Fcitx5:
   - Toàn bộ quá trình phát phím và chờ đợi diễn ra trên luồng tách biệt, do đó Fcitx5 không bao giờ bị đứng hình hay khựng giao diện.

---

## 3. Tại sao Gecko và Terminal Cần Uinput?

1. **Gecko / Zen Browser**:
    - Kiến trúc `libxul.so` của Gecko xử lý `delete_surrounding_text` bất đồng bộ chậm hơn nhịp gõ của người dùng. Dữ liệu ngữ cảnh trả về cho IME thường xuyên bị trễ (stale). Sử dụng Uinput phím cứng khiến Gecko xử lý xóa ở tầng cấp thấp, hoàn toàn tương thích và không bị lỗi.
2. **Terminal (Kitty / Alacritty)**:
    - Các terminal hiện đại trên Wayland chặn hoặc bỏ qua các lệnh xóa ngữ cảnh phức tạp của IME để bảo vệ bộ đệm dòng lệnh. Khi nhận phím xóa từ Uinput, terminal coi đây là phím bàn phím vật lý thực thụ và xử lý ngay tức khắc.
