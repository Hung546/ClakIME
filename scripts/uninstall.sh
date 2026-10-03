#!/usr/bin/env bash
set -euo pipefail

# configuration and defaults
dry_run=0
target_mode="auto"
assume_yes=0

# parse command-line arguments
for arg in "$@"; do
    case "$arg" in
        --dry-run|--simulate|-d)
            dry_run=1
            ;;
        --system|-s)
            target_mode="system"
            ;;
        --user|-u)
            target_mode="user"
            ;;
        --yes|-y)
            assume_yes=1
            ;;
        --help|-h)
            echo "Cách dùng: curl -fsSL https://raw.githubusercontent.com/versenilvis/clak/main/scripts/uninstall.sh | bash [options]"
            echo "Hoặc: bash scripts/uninstall.sh [options]"
            echo ""
            echo "Tùy chọn:"
            echo "  -d, --dry-run, --simulate  Chạy giả lập kiểm tra các mục sẽ gỡ bỏ (không xóa file)"
            echo "  -u, --user                 Chỉ gỡ bỏ bản cài đặt cá nhân (~/.local) [mặc định]"
            echo "  -s, --system               Gỡ bỏ cả bản cài đặt toàn hệ thống (/usr) [cần sudo]"
            echo "  -y, --yes                  Tự động đồng ý mọi thao tác gỡ bỏ"
            echo "  -h, --help                 Hiển thị trợ giúp này"
            exit 0
            ;;
    esac
done

# visual palette and terminal formatting
c_reset="\033[0m"
c_dim="\033[38;5;244m"
c_bold="\033[1m"
c_cyan="\033[1;36m"
c_blue="\033[1;34m"
c_green="\033[1;32m"
c_yellow="\033[1;33m"
c_purple="\033[1;35m"
c_red="\033[1;31m"
c_gray="\033[38;5;240m"
c_accent="\033[38;5;75m"

sep="${c_gray}│${c_reset}"
lbl_check="${c_blue}KIỂM TRA${c_reset}"
lbl_remove="${c_red}GỠ BỎ   ${c_reset}"
lbl_clean="${c_yellow}DỌN DẸP ${c_reset}"
lbl_fcitx="${c_cyan}FCITX5  ${c_reset}"
lbl_wps="${c_yellow}WPS     ${c_reset}"
lbl_warn="${c_yellow}LƯU Ý   ${c_reset}"
lbl_err="${c_red}LỖI     ${c_reset}"

get_ts() {
    printf "%b[%s]%b" "$c_dim" "$(date +%T)" "$c_reset"
}

log_step() {
    local lbl="$1"
    local msg="$2"
    printf "%s %b %b %b\n" "$(get_ts)" "$lbl" "$sep" "$msg"
}

err() {
    log_step "$lbl_err" "${c_red}$1${c_reset}"
    exit 1
}

spin_step() {
    local text="$1"
    if [ ! -t 1 ]; then
        return
    fi
    local frames=("⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏")
    for ((pct=0; pct<=100; pct+=10)); do
        local f=${frames[(pct / 10) % 10]}
        printf "\r\033[38;5;75m%s\033[0m \033[1;36m%3d%%\033[0m \033[38;5;244m%s\033[0m\033[K" "$f" "$pct" "$text"
        sleep 0.03
    done
    printf "\r\033[K"
}

spin_pid() {
    local pid="$1"
    local text="$2"
    if [ ! -t 1 ]; then
        wait "$pid" 2>/dev/null || true
        return
    fi
    local frames=("⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏")
    local i=0
    while kill -0 "$pid" 2>/dev/null; do
        local f=${frames[i % 10]}
        printf "\r\033[38;5;75m%s\033[0m \033[38;5;244m%s\033[0m\033[K" "$f" "$text"
        i=$((i + 1))
        sleep 0.08
    done
    printf "\r\033[K"
}

run_sudo() {
    if [ "$EUID" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        err "Cần quyền quản trị viên (sudo) để thực thi lệnh này"
    fi
}

# simulation flow
run_simulation() {
    log_step "$lbl_check" "Bắt đầu chạy giả lập quy trình gỡ bỏ Clak (không sửa đổi hệ thống)"
    spin_step "Đang quét các thành phần Clak trên hệ thống..."
    
    local has_user=0
    local has_system=0
    local has_wps=0

    [ -f "${HOME}/.local/lib/fcitx5/libclak.so" ] && has_user=1
    [ -f "/usr/lib/fcitx5/libclak.so" ] && has_system=1
    [ -f "${HOME}/.config/environment.d/99-clak-wps.conf" ] && has_wps=1

    echo ""
    echo -e "  ${c_bold}Các mục sẽ được gỡ bỏ:${c_reset}"
    if [ "$has_user" -eq 1 ] || [ "$target_mode" != "system" ]; then
        echo -e "  • Thư viện và cấu hình người dùng: ${c_accent}~/.local/lib/fcitx5/libclak.so${c_reset}"
        echo -e "  • Khai báo addon Fcitx5:            ${c_accent}~/.local/share/fcitx5/addon/clak.conf${c_reset}"
        echo -e "  • Khai báo bộ gõ Fcitx5:           ${c_accent}~/.local/share/fcitx5/inputmethod/clak.conf${c_reset}"
        echo -e "  • Dữ liệu phiên bản & icon:        ${c_accent}~/.local/share/clak/${c_reset}"
    fi

    if [ "$has_system" -eq 1 ] || [ "$target_mode" = "system" ]; then
        echo -e "  • File hệ thống (/usr):             ${c_accent}/usr/lib/fcitx5/libclak.so${c_reset}"
    fi

    if [ "$has_wps" -eq 1 ]; then
        echo -e "  • Cấu hình tương thích WPS Office: ${c_accent}~/.config/environment.d/99-clak-wps.conf${c_reset}"
        echo -e "  • Phím tắt WPS launcher tùy chỉnh: ~/.local/share/applications/wps-office-*.desktop"
    fi

    echo ""
    log_step "$lbl_fcitx" "[Giả lập] Tự động khởi động lại daemon Fcitx5 để giải phóng bộ nhớ"
    echo -e "${c_green}✔ Quá trình giả lập gỡ bỏ hoàn tất thành công!${c_reset}"
    echo ""
}

# execution flow
run_uninstall() {
    # 1. detect installed components
    local has_user_files=0
    local has_sys_files=0

    [ -f "${HOME}/.local/lib/fcitx5/libclak.so" ] && has_user_files=1
    [ -f "${HOME}/.local/share/fcitx5/addon/clak.conf" ] && has_user_files=1
    [ -f "${HOME}/.local/share/clak/version" ] && has_user_files=1

    [ -f "/usr/lib/fcitx5/libclak.so" ] && has_sys_files=1
    [ -f "/usr/share/fcitx5/addon/clak.conf" ] && has_sys_files=1

    if [ "$has_user_files" -eq 0 ] && [ "$has_sys_files" -eq 0 ] && [ "$target_mode" != "system" ]; then
        log_step "$lbl_warn" "Không tìm thấy file cài đặt Clak nào trên hệ thống"
        exit 0
    fi

    # 2. confirm prompt if interactive
    if [ "$assume_yes" -ne 1 ] && [ -t 0 ]; then
        echo -e "${c_bold}Bạn có chắc chắn muốn gỡ bỏ hoàn toàn bộ gõ Clak khỏi máy?${c_reset}"
        read -r -p "Xác nhận gỡ bỏ? [y/N]: " confirm_choice
        case "$confirm_choice" in
            [yY][eE][sS]|[yY])
                ;;
            *)
                echo "Đã hủy thao tác gỡ bỏ."
                exit 0
                ;;
        esac
    fi

    # 3. remove user-space binaries and addons
    if [ "$target_mode" != "system" ] || [ "$has_user_files" -eq 1 ]; then
        spin_step "Đang xóa thư viện và cấu hình cá nhân (~/.local)..."
        rm -f "${HOME}/.local/lib/fcitx5/libclak.so"
        rm -f "${HOME}/.local/share/fcitx5/addon/clak.conf"
        rm -f "${HOME}/.local/share/fcitx5/inputmethod/clak.conf"
        rm -rf "${HOME}/.local/share/clak"
        log_step "$lbl_remove" "Đã xóa file thư viện Clak trong ~/.local"
    fi

    # 4. remove system binaries if requested or detected
    if [ "$target_mode" = "system" ] || ([ "$target_mode" = "auto" ] && [ "$has_sys_files" -eq 1 ]); then
        spin_step "Đang xóa file Clak toàn hệ thống (/usr)..."
        run_sudo rm -f "/usr/lib/fcitx5/libclak.so"
        run_sudo rm -f "/usr/share/fcitx5/addon/clak.conf"
        run_sudo rm -f "/usr/share/fcitx5/inputmethod/clak.conf"
        log_step "$lbl_remove" "Đã xóa file Clak trong hệ thống (/usr)"
    fi

    # 5. cleanup wps office compatibility overrides
    local wps_cleaned=0
    if [ -f "${HOME}/.config/environment.d/99-clak-wps.conf" ]; then
        rm -f "${HOME}/.config/environment.d/99-clak-wps.conf"
        wps_cleaned=1
    fi

    for df in "${HOME}/.local/share/applications"/wps-office-*.desktop; do
        if [ -f "$df" ] && grep -q "QT_IM_MODULE=fcitx" "$df" 2>/dev/null; then
            rm -f "$df"
            wps_cleaned=1
        fi
    done

    for b in wps wpp et wpspdf; do
        local wb="${HOME}/.local/bin/$b"
        if [ -f "$wb" ] && grep -q "QT_IM_MODULE=fcitx" "$wb" 2>/dev/null; then
            rm -f "$wb"
            wps_cleaned=1
        fi
    done

    if [ "$wps_cleaned" -eq 1 ]; then
        command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
        log_step "$lbl_wps" "Đã dọn dẹp các cấu hình tương thích WPS Office"
    fi

    # 6. cleanup icons
    spin_step "Đang dọn dẹp icon Clak..."
    find "${HOME}/.local/share/icons" -type f -name "*clak*" -delete 2>/dev/null || true
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -f -q -t "${HOME}/.local/share/icons/hicolor" 2>/dev/null || true
    fi
    log_step "$lbl_clean" "Đã dọn dẹp biểu tượng Clak trong hệ thống"

    # 7. reload fcitx5
    if command -v fcitx5 >/dev/null 2>&1; then
        (
            fcitx5 -r -d >/dev/null 2>&1 || true
            sleep 0.4
        ) &
        local reload_pid=$!
        spin_pid "$reload_pid" "Đang khởi động lại daemon Fcitx5..."
        wait "$reload_pid" 2>/dev/null || true
        log_step "$lbl_fcitx" "Đã khởi động lại Fcitx5 thành công"
    fi

    printf "\r\033[K\n"
    echo -e "${c_green}✔ Đã gỡ bỏ hoàn toàn bộ gõ Clak khỏi hệ thống!${c_reset}"
    echo ""
}

# main router
if [ "$dry_run" -eq 1 ]; then
    run_simulation
else
    run_uninstall
fi
