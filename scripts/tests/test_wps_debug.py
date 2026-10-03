#!/usr/bin/env python3
import os
import sys
import time
import fcntl
import struct
import subprocess
import re

CYAN = "\033[1;36m"
GREEN = "\033[1;32m"
YELLOW = "\033[1;33m"
RED = "\033[1;31m"
MAGENTA = "\033[1;35m"
DIM = "\033[38;5;244m"
BOLD = "\033[1m"
RESET = "\033[0m"

UI_SET_EVBIT = 0x40045564
UI_SET_KEYBIT = 0x40045565
UI_DEV_SETUP = 0x405c5503
UI_DEV_CREATE = 0x5501
UI_DEV_DESTROY = 0x5502

EV_SYN = 0
EV_KEY = 1
SYN_REPORT = 0

KEY_MAP = {
    'a': 30, 'b': 48, 'c': 46, 'd': 32, 'e': 18, 'f': 33, 'g': 34,
    'h': 35, 'i': 23, 'j': 36, 'k': 37, 'l': 38, 'm': 50, 'n': 49,
    'o': 24, 'p': 25, 'q': 16, 'r': 19, 's': 31, 't': 20, 'u': 22,
    'v': 47, 'w': 17, 'x': 45, 'y': 21, 'z': 44, ' ': 57, '\n': 28,
    '1': 2, '2': 3, '3': 4, '4': 5, '5': 6, '6': 7, '7': 8, '8': 9, '9': 10, '0': 11,
    '-': 12, '=': 13, '[': 26, ']': 27, ';': 39, "'": 40, ',': 51, '.': 52, '/': 53
}

def run_cmd(cmd):
    try:
        return subprocess.check_output(cmd, shell=True, text=True, stderr=subprocess.DEVNULL).strip()
    except subprocess.CalledProcessError:
        return ""

class VirtualKeyboard:
    def __init__(self):
        self.fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
        fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_KEY)
        fcntl.ioctl(self.fd, UI_SET_EVBIT, EV_SYN)
        for code in range(1, 128):
            fcntl.ioctl(self.fd, UI_SET_KEYBIT, code)

        name = b'Clak-Debug-Keybd\x00'.ljust(80, b'\x00')
        setup_data = struct.pack('HHHH', 0x03, 0x1234, 0x5678, 1) + name + struct.pack('I', 0)
        fcntl.ioctl(self.fd, UI_DEV_SETUP, setup_data)
        fcntl.ioctl(self.fd, UI_DEV_CREATE)
        time.sleep(0.2)

    def emit(self, event_type, code, value):
        ev = struct.pack('llHHI', 0, 0, event_type, code, value)
        os.write(self.fd, ev)

    def type_key(self, code, press_duration=0.005, delay_after=0.015):
        self.emit(EV_KEY, code, 1)
        self.emit(EV_SYN, SYN_REPORT, 0)
        time.sleep(press_duration)
        self.emit(EV_KEY, code, 0)
        self.emit(EV_SYN, SYN_REPORT, 0)
        time.sleep(delay_after)

    def type_text(self, text, delay_after=0.015):
        for ch in text:
            lower = ch.lower()
            if lower in KEY_MAP:
                self.type_key(KEY_MAP[lower], delay_after=delay_after)

    def close(self):
        if self.fd >= 0:
            fcntl.ioctl(self.fd, UI_DEV_DESTROY)
            os.close(self.fd)
            self.fd = -1

def main():
    print(f"\n{BOLD}{CYAN}=== CLAK WPS OFFICE TYPING BENCHMARK & DEBUG TOOL ==={RESET}\n")

    wps_pid = run_cmd("pgrep -f '/office6/wps'")
    if not wps_pid:
        wps_pid = run_cmd("pgrep -f 'wps'")
    
    if not wps_pid:
        print(f"{RED}❌ Không tìm thấy tiến trình WPS Office đang chạy!{RESET}")
        print("Vui lòng mở WPS Writer trước khi chạy script test.")
        sys.exit(1)
    
    wps_pid = wps_pid.split()[0]
    print(f"  • Tiến trình WPS Office: {GREEN}PID {wps_pid}{RESET}")

    environ_path = f"/proc/{wps_pid}/environ"
    qt_im = "chưa đặt"
    xmod = "chưa đặt"
    try:
        with open(environ_path, "rb") as f:
            raw = f.read().split(b"\0")
            for item in raw:
                s = item.decode("utf-8", errors="ignore")
                if s.startswith("QT_IM_MODULE="):
                    qt_im = s.split("=", 1)[1]
                elif s.startswith("XMODIFIERS="):
                    xmod = s.split("=", 1)[1]
    except Exception as e:
        print(f"  {YELLOW}• Không đọc được environ: {e}{RESET}")

    print(f"  • Biến QT_IM_MODULE: {CYAN}{qt_im}{RESET}")
    print(f"  • Biến XMODIFIERS:   {CYAN}{xmod}{RESET}")

    maps_path = f"/proc/{wps_pid}/maps"
    loaded_plugin = "unknown"
    try:
        with open(maps_path, "r", errors="ignore") as f:
            content = f.read()
            if "libfcitxplatforminputcontextplugin.so" in content:
                loaded_plugin = "fcitx plugin (tốt)"
            elif "libcomposeplatforminputcontextplugin.so" in content:
                loaded_plugin = "compose plugin (lỗi: không nhận phím fcitx)"
    except Exception as e:
        pass
    print(f"  • Plugin Input nạp trong RAM: {GREEN if 'fcitx' in loaded_plugin else RED}{loaded_plugin}{RESET}")

    cur_im = run_cmd("fcitx5-remote -n")
    if cur_im != "clak":
        print(f"  • Chuyển bộ gõ sang Clak: {YELLOW}fcitx5-remote -s clak{RESET}")
        run_cmd("fcitx5-remote -s clak")
    else:
        print(f"  • Bộ gõ Fcitx5 hiện tại: {GREEN}clak{RESET}")

    try:
        vk = VirtualKeyboard()
    except Exception as e:
        print(f"{RED}❌ Không thể mở /dev/uinput: {e}{RESET}")
        print("Hãy đảm bảo người dùng có quyền trong nhóm input (sudo usermod -aG input $USER)")
        sys.exit(1)

    print(f"\n{BOLD}[1/3] Kích hoạt và chuyển sang cửa sổ WPS Office...{RESET}")
    run_cmd("hyprctl dispatch 'hl.dsp.focus({ workspace = 3 })'")
    time.sleep(0.5)

    test_suites = [
        {
            "name": "Kiểm tra cơ bản (Tốc độ tiêu chuẩn 50 WPM ~ 30ms)",
            "delay": 0.030,
            "cases": [
                ("ddaa ", "đâ"),
                ("xin chaof ", "xin chào"),
                ("ddaay laf moojt baif kieemr tra ", "đây là một bài kiểm tra")
            ]
        },
        {
            "name": "Kiểm tra gõ nhanh & ghép âm phức tạp (100 WPM ~ 15ms)",
            "delay": 0.015,
            "cases": [
                ("toocs ddoo gox tieengs vieejt muowjt maf ", "tốc độ gõ tiếng việt mượt mà"),
                ("nghieeng ngarr khueechs ddaaji ", "nghiêng ngả khuếch đại")
            ]
        },
        {
            "name": "Stress test gõ cực nhanh (150+ WPM ~ 8ms)",
            "delay": 0.008,
            "cases": [
                ("tieengs vieejt trong suoots muowjt maf ", "tiếng việt trong suốt mượt mà")
            ]
        }
    ]

    log_path = "/tmp/clak.log"
    start_pos = 0
    if os.path.exists(log_path):
        start_pos = os.path.getsize(log_path)

    print(f"{BOLD}[2/3] Bắt đầu gõ qua bàn phím ảo phần cứng uinput evdev...{RESET}\n")
    time.sleep(0.3)

    # Enter new line first in WPS
    vk.type_key(KEY_MAP['\n'], delay_after=0.1)

    for suite in test_suites:
        print(f"  {MAGENTA}▶ {suite['name']}{RESET}")
        for raw_text, expected in suite['cases']:
            print(f"    • Gõ: {CYAN}{raw_text}{RESET} -> Kỳ vọng: {GREEN}{expected}{RESET}")
            vk.type_text(raw_text, delay_after=suite['delay'])
            time.sleep(0.2)
        print()

    vk.close()
    time.sleep(0.5)

    print(f"{BOLD}[3/3] Phân tích log độ trễ và sự kiện...{RESET}")
    run_cmd("hyprctl dispatch 'hl.dsp.focus({ workspace = 2 })'")

    if not os.path.exists(log_path):
        print(f"{RED}Không tìm thấy {log_path}{RESET}")
        sys.exit(1)

    with open(log_path, "r", errors="ignore") as f:
        f.seek(start_pos)
        new_lines = f.readlines()

    latencies = []
    replace_count = 0
    forward_count = 0
    timeouts = []
    replays = []
    uinput_waits = []

    for line in new_lines:
        line = line.strip()
        if "[LATENCY]" in line:
            m = re.search(r'delta_ms=([0-9\.]+)\s+action=([^\s]+)', line)
            if m:
                d_ms = float(m.group(1))
                act = m.group(2)
                latencies.append((d_ms, act, line))
                if act.startswith("REPLACE"):
                    replace_count += 1
                elif act == "FORWARD":
                    forward_count += 1
        elif "SENTINEL TIMEOUT" in line:
            timeouts.append(line)
        elif "replayBufferedKeys" in line:
            replays.append(line)
        elif "uinput waiting for sentinel" in line:
            uinput_waits.append(line)

    print(f"\n{BOLD}=== KẾT QUẢ PHÂN TÍCH HIỆU NĂNG WPS OFFICE ==={RESET}")
    print(f"  • Tổng sự kiện phím ghi nhận: {BOLD}{len(latencies)}{RESET} ({forward_count} FORWARD, {replace_count} REPLACE)")
    print(f"  • Lượt gửi phím qua uinput:    {BOLD}{len(uinput_waits)}{RESET}")
    print(f"  • Số lần buffer phím (replay): {BOLD}{len(replays)}{RESET}")
    print(f"  • Số lần timeout sentinel:     {GREEN if len(timeouts) == 0 else RED}{len(timeouts)}{RESET}")

    replace_latencies = [d for d, act, _ in latencies if act.startswith("REPLACE")]
    if replace_latencies:
        replace_latencies.sort()
        avg_lat = sum(replace_latencies) / len(replace_latencies)
        p50 = replace_latencies[len(replace_latencies) // 2]
        p95 = replace_latencies[int(len(replace_latencies) * 0.95)]
        max_lat = replace_latencies[-1]
        min_lat = replace_latencies[0]

        print(f"\n{BOLD}Thống kê độ trễ REPLACE (xóa và thay chữ tiếng Việt):{RESET}")
        print(f"  • Tối thiểu (min): {GREEN}{min_lat:.2f} ms{RESET}")
        print(f"  • Trung bình (avg): {GREEN if avg_lat < 10 else YELLOW}{avg_lat:.2f} ms{RESET}")
        print(f"  • Trung vị (p50):   {GREEN if p50 < 10 else YELLOW}{p50:.2f} ms{RESET}")
        print(f"  • Phân vị (p95):    {GREEN if p95 < 20 else RED}{p95:.2f} ms{RESET}")
        print(f"  • Tối đa (max):    {GREEN if max_lat < 25 else RED}{max_lat:.2f} ms{RESET}")
    else:
        print(f"  {YELLOW}Không có sự kiện REPLACE nào được ghi nhận trong phiên test.{RESET}")

    if timeouts:
        print(f"\n{RED}CẢNH BÁO: Có {len(timeouts)} lần uinput sentinel bị timeout!{RESET}")
        for t in timeouts[:3]:
            print(f"  {RED}↳ {t}{RESET}")

    if replays:
        print(f"\n{YELLOW}LƯU Ý: Có {len(replays)} lần phím phải chờ trong buffer do luồng xóa uinput:{RESET}")
        for r in replays[:5]:
            print(f"  {DIM}↳ {r}{RESET}")

    print("\n" + "=" * 55 + "\n")

if __name__ == "__main__":
    main()
