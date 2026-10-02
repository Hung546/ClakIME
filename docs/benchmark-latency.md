# Đo đạc và Tối ưu Độ trễ (Benchmark & Latency)

Thư mục: [scripts/tests/](file:///home/verse/dev/github/input-method/scripts/tests/)

File chính: [benchmark_all_groups.sh](file:///home/verse/dev/github/input-method/scripts/tests/benchmark_all_groups.sh), [analyze_latency.py](file:///home/verse/dev/github/input-method/scripts/tests/analyze_latency.py)

Để đảm bảo cảm giác gõ mượt mà và không dựa trên phỏng đoán, Clak tích hợp cơ chế đo độ trễ thực thời trên từng hành vi gõ phím.

---

## 1. Phương pháp Đo Latency Thực tế

Độ trễ được tính chính xác giữa 2 mốc thời gian:

1. **Mốc bắt đầu (`op_start_us_`)**: Ghi nhận ngay khi sự kiện nhấn phím vật lý (`keyEvent`) từ bàn phím người dùng đi vào Fcitx5 qua đồng hồ `CLOCK_MONOTONIC`.
2. **Mốc kết thúc**: Ghi nhận ngay sau khi chuỗi ký tự tiếng Việt cuối cùng được gửi ra ứng dụng qua `commitString` (đối với Uinput là lúc phím chốt hoàn tất chu trình).

Delta thời gian (tính bằng milli-giây) được ghi vào log theo định dạng:

```text
[LATENCY] group=<Tên-Nhóm> delta_ms=<Số_ms> action=REPLACE
```

---

## 2. Bảng Thống kê Latency Thực tế Trên 5 Nhóm Ứng dụng

Dữ liệu thực nghiệm thu thập từ hơn 50 đến 100 lần thay thế từ ngữ cảnh tự nhiên trên từng nhóm ứng dụng với tốc độ gõ chuẩn người thật (80ms/phím ~ 65 WPM):

| Nhóm ứng dụng                  | Số mẫu | p50 (ms) | p95 (ms) | p99 (ms) | Nhỏ nhất (ms) | Lớn nhất (ms) |
| :----------------------------- | :----: | :------: | :------: | :------: | :-----------: | :-----------: |
| **Chromium-SurroundingText**   |   97   |   0.30   |   0.63   |   0.82   |     0.11      |     1.09      |
| **address-bar (Omnibox)**      |   50   |   0.27   |   0.55   |   0.75   |     0.13      |     0.85      |
| **Docs-Uinput (Google Docs)**  |   97   |   5.33   |  18.73   |  21.07   |     0.17      |     21.27     |
| **Terminal-Uinput (Kitty)**    |   97   |   6.44   |  15.52   |  17.84   |     0.17      |     21.37     |
| **Gecko-Uinput (Zen Browser)** |   97   |   5.41   |   7.19   |   7.77   |     0.16      |     8.83      |

### Nhận xét:

- Mắt người bình thường chỉ bắt đầu nhận biết độ trễ khi vượt ngưỡng xấp xỉ 100ms.
- Toàn bộ 5 nhóm ứng dụng của Clak đều có p95 dưới 20ms, và p50 dao động từ 0.3ms đến 6ms. Người dùng hoàn toàn không cảm nhận được bất kỳ sự giật lag nào.

---

## 3. Cách Tự Chạy Lại Bộ Benchmark

### Bước 1: Khởi động benchmark tự động

```bash
# chạy benchmark toàn diện với nhịp gõ 80ms/phím
bash scripts/tests/benchmark_all_groups.sh 80
```

### Bước 2: Phân tích kết quả từ file log

```bash
python3 scripts/tests/analyze_latency.py /tmp/clak.log
```

---

## 4. Quản lý và Tắt Log

Để bảo đảm hiệu năng tối đa và không tốn dung lượng ổ đĩa:

- File log `/tmp/clak.log` tự động bị giới hạn tối đa **2 MB**. Khi vượt quá dung lượng này, file sẽ tự động được làm mới (truncate).
- Có thể tắt hoàn toàn tính năng ghi log bằng biến môi trường:
    ```bash
    export CLAK_LOG=0
    ```
