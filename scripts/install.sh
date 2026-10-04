#!/usr/bin/env bash
set -euo pipefail

# configuration and defaults
repo="versenilvis/clak"
api_url="${CLAK_API_URL:-https://api.github.com}"
release_tag="${CLAK_RELEASE_TAG:-}"
dry_run=0
demo_uinput=0
target_mode="user"

# parse command-line arguments
for arg in "$@"; do
    case "$arg" in
        --dry-run|--simulate|-d)
            dry_run=1
            demo_uinput=1
            ;;
        --demo-uinput)
            demo_uinput=1
            ;;
        --skip-uinput)
            demo_uinput=0
            ;;
        --system|-s)
            target_mode="system"
            ;;
        --user|-u)
            target_mode="user"
            ;;
        --tag=*)
            release_tag="${arg#*=}"
            ;;
        --help|-h)
            echo "Cách dùng: curl -fsSL https://raw.githubusercontent.com/versenilvis/clak/main/scripts/install.sh | bash [options]"
            echo "Hoặc: bash scripts/install.sh [options]"
            echo ""
            echo "Tùy chọn:"
            echo "  -d, --dry-run, --simulate  Chạy giả lập kiểm tra giao diện và quy trình (không sửa hệ thống)"
            echo "      --demo-uinput          Giả lập tình huống cần cấu hình quyền /dev/uinput"
            echo "  -u, --user                 Cài vào thư mục người dùng (~/.local) [mặc định, không cần sudo]"
            echo "  -s, --system               Cài vào toàn hệ thống (/usr) [sẽ hỏi sudo khi chép file]"
            echo "      --tag=<phiên bản>      Chỉ định phiên bản release cần cài (ví dụ: v0.1.0)"
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
lbl_fetch="${c_yellow}TẢI VỀ  ${c_reset}"
lbl_install="${c_green}CÀI ĐẶT ${c_reset}"
lbl_uinput="${c_purple}UINPUT  ${c_reset}"
lbl_fcitx="${c_cyan}FCITX5  ${c_reset}"
lbl_wps="${c_yellow}WPS     ${c_reset}"
lbl_warn="${c_yellow}LƯU Ý   ${c_reset}"
lbl_err="${c_red}LỖI     ${c_reset}"

# format timestamp
get_ts() {
    printf "%b[%s]%b" "$c_dim" "$(date +%T)" "$c_reset"
}

# formatted log line
log_step() {
    local lbl="$1"
    local msg="$2"
    printf "%s %b %b %b\n" "$(get_ts)" "$lbl" "$sep" "$msg"
}

# in-place animated spinner with percentage
spin_step() {
    local text="$1"
    local duration="${2:-0.5}"
    if [ ! -t 1 ]; then
        return
    fi
    local frames=("⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏")
    for ((pct=0; pct<=100; pct+=10)); do
        local f=${frames[(pct / 10) % 10]}
        printf "\r\033[38;5;75m%s\033[0m \033[1;36m%3d%%\033[0m \033[38;5;244m%s\033[0m\033[K" "$f" "$pct" "$text"
        sleep 0.04
    done
    printf "\r\033[K"
}

# in-place animated download spinner with percentage and kb counter
spin_download() {
    local text="$1"
    local total_kb="${2:-1843}"
    local track_pid="${3:-}"
    local track_file="${4:-}"

    if [ ! -t 1 ]; then
        if [ -n "$track_pid" ]; then
            wait "$track_pid" 2>/dev/null || true
        fi
        return
    fi

    local frames=("⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏")

    if [ -n "$track_pid" ] && [ -n "$track_file" ]; then
        local i=0
        while kill -0 "$track_pid" 2>/dev/null; do
            local f=${frames[i % 10]}
            local cur_bytes=0
            if [ -f "$track_file" ]; then
                cur_bytes=$(wc -c < "$track_file" 2>/dev/null || echo 0)
            fi
            local cur_kb=$((cur_bytes / 1024))
            local pct=$((cur_kb * 100 / total_kb))
            [ $pct -gt 99 ] && pct=99
            printf "\r\033[38;5;75m%s\033[0m \033[1;36m%3d%%\033[0m \033[38;5;244m%s (%d/%d KB)\033[0m\033[K" \
                "$f" "$pct" "$text" "$cur_kb" "$total_kb"
            i=$((i + 1))
            sleep 0.08
        done
        wait "$track_pid" 2>/dev/null || true
        local f=${frames[i % 10]}
        printf "\r\033[38;5;75m%s\033[0m \033[1;36m100%%\033[0m \033[38;5;244m%s (%d/%d KB)\033[0m\033[K" \
            "$f" "$text" "$total_kb" "$total_kb"
        sleep 0.04
    else
        for ((pct=0; pct<=100; pct+=4)); do
            local f=${frames[(pct / 4) % 10]}
            local cur_kb=$((total_kb * pct / 100))
            printf "\r\033[38;5;75m%s\033[0m \033[1;36m%3d%%\033[0m \033[38;5;244m%s (%d/%d KB)\033[0m\033[K" \
                "$f" "$pct" "$text" "$cur_kb" "$total_kb"
            sleep 0.04
        done
    fi
    printf "\r\033[K"
}

# in-place animated spinner for asynchronous process
spin_pid() {
    local pid=$1
    local text=$2
    local expected_sec="${3:-1}"
    if [ ! -t 1 ]; then
        wait "$pid" 2>/dev/null || true
        return
    fi
    local frames=("⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏")
    local i=0
    local pct=0
    while kill -0 "$pid" 2>/dev/null; do
        local f=${frames[i % 10]}
        pct=$((i * 100 / (expected_sec * 12)))
        [ $pct -gt 95 ] && pct=95
        printf "\r\033[38;5;75m%s\033[0m \033[1;36m%3d%%\033[0m \033[38;5;244m%s\033[0m\033[K" "$f" "$pct" "$text"
        i=$((i + 1))
        sleep 0.08
    done
    wait "$pid" 2>/dev/null || true
    local f=${frames[i % 10]}
    printf "\r\033[38;5;75m%s\033[0m \033[1;36m100%%\033[0m \033[38;5;244m%s\033[0m\033[K" "$f" "$text"
    sleep 0.04
    printf "\r\033[K"
}

# run command with sudo via tty
run_sudo() {
    if [ -e /dev/tty ]; then
        sudo "$@" < /dev/tty
    else
        sudo "$@"
    fi
}

# error exit
err() {
    log_step "$lbl_err" "$1"
    exit 1
}

# detect architecture
get_arch() {
    local os
    local arch
    os=$(uname -s | tr '[:upper:]' '[:lower:]')
    arch=$(uname -m)

    if [ "$os" != "linux" ]; then
        err "Hệ điều hành không hỗ trợ: $os (Clak chỉ hỗ trợ Linux)"
    fi

    case "$arch" in
        x86_64|amd64)
            echo "linux-x86_64"
            ;;
        *)
            err "Kiến trúc CPU không hỗ trợ: $arch (hiện tại hỗ trợ x86_64)"
            ;;
    esac
}

# explain why sudo is required for uinput
print_uinput_sudo_notice() {
    echo ""
    echo -e "  ${c_red}╭─ CẦN QUYỀN SUDO: CẤU HÌNH /dev/uinput ─────────────────────╮${c_reset}"
    echo -e "  ${c_red}│${c_reset}  Clak dùng thiết bị ảo ${c_bold}/dev/uinput${c_reset} để gửi phím trực tiếp,  ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}  giúp ${c_green}chống nuốt chữ và mất dấu${c_reset} trên các ứng dụng:         ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}   • Terminal: Kitty, Ghostty, Alacritty                    ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}   • Trình duyệt: Zen Browser, Firefox Gecko, Chromium      ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}   • Họ VSCode: Antigravity IDE, Cursor, VSCode, Windsurf   ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}   • Và nhiều thứ khác nữa                                  ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}                                                            ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}  Thao tác: Tạo file ${c_cyan}/etc/udev/rules.d/99-uinput.rules${c_reset}      ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}  Hệ thống sẽ yêu cầu mật khẩu sudo bên dưới để kích hoạt.  ${c_red}│${c_reset}"
    echo -e "  ${c_red}╰────────────────────────────────────────────────────────────╯${c_reset}"
    echo ""
}

# explain why sudo is required for system install
print_system_sudo_notice() {
    echo ""
    echo -e "  ${c_red}╭─ CẦN QUYỀN SUDO: CÀI VÀO THƯ MỤC HỆ THỐNG ─────────────────╮${c_reset}"
    echo -e "  ${c_red}│${c_reset}  Bạn đã chọn cài đặt Clak vào thư mục: ${c_bold}/usr/lib/fcitx5${c_reset}     ${c_red}│${c_reset}"
    echo -e "  ${c_red}│${c_reset}  Cần sudo ở bước này để sao chép file addon vào hệ thống.  ${c_red}│${c_reset}"
    echo -e "  ${c_red}╰────────────────────────────────────────────────────────────╯${c_reset}"
    echo ""
}

# detect and configure wps office compatibility
configure_wps_compatibility() {
    local is_sim="${1:-0}"
    local has_wps=0
    if command -v wps >/dev/null 2>&1 || [ -d /usr/lib/office6 ] || compgen -G "/usr/share/applications/wps-office-*.desktop" >/dev/null; then
        has_wps=1
    fi

    if [ "$has_wps" -eq 1 ]; then
        log_step "$lbl_wps" "Phát hiện hệ thống có cài đặt WPS Office (ứng dụng Qt5 XWayland)"
        echo -e "  ${c_yellow}• WPS Office sử dụng Qt5 nội bộ, cần biến QT_IM_MODULE=fcitx để nạp bộ gõ${c_reset}"
        echo -e "  ${c_yellow}• Clak tự động whitelist WPS: kích hoạt uinput trực tiếp và lọc Surrounding Text rác ('10')${c_reset}"
        echo -e "  ${c_yellow}• Hoàn toàn không ghi đè file gốc của WPS, không ảnh hưởng gì tới app chính${c_reset}"
        echo -e "  ${c_yellow}• Bạn vẫn có thể sử dụng và cập nhật WPS Office bình thường${c_reset}"

        if [ "$is_sim" -eq 1 ]; then
            echo -e "  ${c_yellow}  [Giả lập] Tối ưu shortcut WPS tại ~/.local/share/applications/ (an toàn khi update hệ thống)${c_reset}"
            log_step "$lbl_wps" "Đã tối ưu tương thích WPS Office (${c_green}sử dụng được ngay với mọi launcher${c_reset})"
        else
            mkdir -p "${HOME}/.local/share/applications"
            local patched_count=0
            for df in /usr/share/applications/wps-office-*.desktop; do
                if [ -f "$df" ]; then
                    local bname
                    bname=$(basename "$df")
                    sed 's|^Exec=/usr/bin/|Exec=env QT_IM_MODULE=fcitx /usr/bin/|' "$df" > "${HOME}/.local/share/applications/${bname}"
                    patched_count=$((patched_count + 1))
                fi
            done
            if [ "$patched_count" -gt 0 ]; then
                command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
                log_step "$lbl_wps" "Đã tạo shortcut tương thích tại ${c_accent}~/.local/share/applications/${c_reset} (${c_green}${patched_count} ứng dụng${c_reset})"
            fi
            # configure systemd environment.d to export QT_IM_MODULE=fcitx
            mkdir -p "${HOME}/.config/environment.d"
            echo "QT_IM_MODULE=fcitx" > "${HOME}/.config/environment.d/99-clak-wps.conf"
            log_step "$lbl_wps" "Đã khai báo biến ${c_accent}QT_IM_MODULE=fcitx${c_reset} trong environment.d (${c_green}hoàn toàn không ghi đè file gốc của WPS${c_reset})"

            # create user-level terminal wrappers in ~/.local/bin for terminal usage
            mkdir -p "${HOME}/.local/bin"
            local bin_count=0
            for b in wps wpp et wpspdf; do
                if command -v "/usr/bin/$b" >/dev/null 2>&1; then
                    cat << 'EOF' > "${HOME}/.local/bin/$b"
#!/bin/sh
exec env QT_IM_MODULE=fcitx "/usr/bin/$(basename "$0")" "$@"
EOF
                    chmod +x "${HOME}/.local/bin/$b"
                    bin_count=$((bin_count + 1))
                fi
            done
            if [ "$bin_count" -gt 0 ]; then
                log_step "$lbl_wps" "Đã tạo wrapper tương thích tại ${c_accent}~/.local/bin/${c_reset} (${c_green}${bin_count} lệnh${c_reset})"
            fi
        fi
    fi
}

# configure autostart with system
configure_autostart() {
    local is_sim="${1:-0}"
    if [ "$is_sim" -eq 1 ]; then
        log_step "$lbl_install" "[Giả lập] Tự động bật khởi động Clak Tiếng Việt cùng hệ thống"
        return
    fi

    local autostart_dir="${HOME}/.config/autostart"
    mkdir -p "$autostart_dir"
    cat << 'EOF' > "${autostart_dir}/clak-autostart.desktop"
[Desktop Entry]
Type=Application
Name=Clak Vietnamese Input Method
Comment=Autostart Fcitx5 with Clak input method
Exec=fcitx5 -d
Icon=org.fcitx.Fcitx5
Terminal=false
Categories=System;Utility;
StartupNotify=false
X-GNOME-Autostart-Phase=Applications
X-GNOME-AutoRestart=true
X-GNOME-Autostart-Notify=false
X-KDE-autostart-after=panel
EOF

    local env_dir="${HOME}/.config/environment.d"
    mkdir -p "$env_dir"
    cat << 'EOF' > "${env_dir}/99-clak-im.conf"
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
INPUT_METHOD=fcitx5
SDL_IM_MODULE=fcitx
EOF

    local profile_file="${HOME}/.config/fcitx5/profile"
    if [ ! -f "$profile_file" ]; then
        mkdir -p "${HOME}/.config/fcitx5"
        cat << 'EOF' > "$profile_file"
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=clak

[Groups/0/Items/0]
Name=keyboard-us

[Groups/0/Items/1]
Name=clak

[GroupOrder]
0=Default
EOF
    else
        if ! grep -q "Name=clak" "$profile_file" 2>/dev/null; then
            local count
            count=$(grep -c "\[Groups/0/Items/" "$profile_file" 2>/dev/null || echo "1")
            sed -i "/\[GroupOrder\]/i \\\n[Groups/0/Items/${count}]\nName=clak\n" "$profile_file" 2>/dev/null || true
        fi
        sed -i 's/^DefaultIM=.*/DefaultIM=clak/' "$profile_file" 2>/dev/null || true
    fi

    log_step "$lbl_install" "Đã kích hoạt tự động chạy Clak Tiếng Việt cùng hệ thống"
}

# simulation flow
run_simulation() {
    # 1. detect os and arch
    local arch
    arch=$(uname -m)
    log_step "$lbl_check" "Hệ thống hợp lệ: ${c_purple}linux-${arch}${c_reset}"

    # 2. check fcitx5
    if command -v fcitx5 >/dev/null 2>&1; then
        local f_ver
        f_ver=$(fcitx5 --version 2>/dev/null | head -n1 || echo "fcitx5")
        log_step "$lbl_check" "Đã phát hiện Fcitx5: ${c_cyan}${f_ver}${c_reset}"
    else
        log_step "$lbl_warn" "Chưa tìm thấy Fcitx5. Vui lòng cài đặt: ${c_yellow}sudo pacman -S fcitx5${c_reset}"
    fi

    # 3. simulated version tag
    local sim_tag="${release_tag:-v0.1.0}"
    log_step "$lbl_fetch" "Phiên bản cài đặt: ${c_cyan}${sim_tag}${c_reset}"

    # 4. simulated download
    local bundle_name="clak-${sim_tag#v}-linux-${arch}.tar.gz"
    spin_download "Đang tải ${bundle_name} (~1.8 MB)" 1843
    log_step "$lbl_fetch" "Tải về thành công · Mã băm ${c_green}SHA256${c_reset} chính xác"

    # 5. simulated install destination
    local dest_dir="${HOME}/.local/lib/fcitx5"
    if [ "$target_mode" = "system" ]; then
        dest_dir="/usr/lib/fcitx5"
        print_system_sudo_notice
        echo -e "  ${c_dim}[Giả lập] Sẽ hỏi mật khẩu sudo tại đây để sao chép file${c_reset}"
    fi
    log_step "$lbl_install" "Đã cài ${c_accent}${dest_dir}/libclak.so${c_reset}"
    log_step "$lbl_install" "Đã cài ${c_accent}${dest_dir%/lib*}/share/fcitx5/addon/clak.conf${c_reset}"
    log_step "$lbl_install" "Đã cài ${c_accent}${dest_dir%/lib*}/share/fcitx5/inputmethod/clak.conf${c_reset}"

    # 6. check uinput permissions
    if [ "$demo_uinput" -eq 1 ] || [ ! -w /dev/uinput ]; then
        log_step "$lbl_warn" "Người dùng hiện tại chưa có quyền ghi vào /dev/uinput"
        print_uinput_sudo_notice
        echo -e "  ${c_dim}[Giả lập] Bạn chỉ cần nhập mật khẩu sudo tại đây khi chạy thật${c_reset}"
        spin_step "Đang áp dụng rule udev cho /dev/uinput..." 0.5
        log_step "$lbl_uinput" "Đã cấu hình quyền uinput thành công"
    else
        log_step "$lbl_uinput" "Quyền truy cập /dev/uinput đã sẵn sàng (${c_green}sử dụng được ngay${c_reset})"
    fi

    # 7. check wps compatibility
    configure_wps_compatibility 1

    # 8. check and configure autostart
    configure_autostart 1

    # 9. reload fcitx5 daemon with final spinner
    spin_step "Đang nạp lại daemon Fcitx5..." 0.6

    # final line directly after spin completes
    printf "\r\033[K\n"
    echo -e "${c_green}✔ Cài đặt Clak thành công vào hệ thống Fcitx5!${c_reset}"
    echo -e "  Vào cấu hình Fcitx5 để thêm Clak vào danh sách bộ gõ"
    echo ""
}

# actual installation flow
run_install() {
    # 1. detect architecture
    local arch
    arch=$(get_arch)
    log_step "$lbl_check" "Kiến trúc phần cứng: ${c_purple}${arch}${c_reset}"

    # 2. check curl or wget
    local has_curl=0
    local has_wget=0
    if command -v curl >/dev/null 2>&1; then
        has_curl=1
    elif command -v wget >/dev/null 2>&1; then
        has_wget=1
    else
        err "Cần có curl hoặc wget để tải bộ cài Clak"
    fi

    # 3. get download url from github
    local release_path="/releases/latest"
    if [ -n "$release_tag" ]; then
        release_path="/releases/tags/${release_tag}"
    fi

    local tmp_response
    tmp_response=$(mktemp)

    (
        if [ "$has_curl" -eq 1 ]; then
            curl -sL -w "\n%{http_code}" \
                ${GITHUB_TOKEN:+-H "Authorization: Bearer ${GITHUB_TOKEN}"} \
                "${api_url}/repos/${repo}${release_path}" > "$tmp_response"
        else
            wget -q -O "$tmp_response" \
                ${GITHUB_TOKEN:+--header "Authorization: Bearer ${GITHUB_TOKEN}"} \
                "${api_url}/repos/${repo}${release_path}"
            echo "200" >> "$tmp_response"
        fi
    ) &
    spin_pid $! "Đang truy vấn phiên bản phát hành từ GitHub..." 1

    local http_code
    http_code=$(tail -n1 "$tmp_response" 2>/dev/null || echo "200")
    local releases_json
    releases_json=$(sed '$d' "$tmp_response" 2>/dev/null || cat "$tmp_response")
    rm -f "$tmp_response"

    if [ "$http_code" = "404" ]; then
        err "Không tìm thấy bản phát hành nào trên GitHub. Có thể dự án chưa công bố release."
    fi

    local tag_name
    tag_name=$(echo "$releases_json" | grep '"tag_name":' | head -1 | cut -d '"' -f 4 || echo "$release_tag")
    local download_url
    download_url=$(echo "$releases_json" | grep "browser_download_url" | grep "${arch}\.tar\.gz" | head -1 | cut -d '"' -f 4 || true)

    if [ -z "$download_url" ]; then
        err "Không tìm thấy file nén '${arch}.tar.gz' trong bản phát hành ${tag_name}"
    fi

    log_step "$lbl_fetch" "Phiên bản chỉ định: ${c_cyan}${tag_name}${c_reset}"

    # 4. download release archive
    local tmp_dir
    tmp_dir=$(mktemp -d)
    trap 'rm -rf "${tmp_dir}"' EXIT

    local archive_name
    archive_name=$(basename "$download_url")

    (
        cd "$tmp_dir"
        if [ "$has_curl" -eq 1 ]; then
            curl -sLO "$download_url"
        else
            wget -q "$download_url"
        fi
    ) &
    local dl_pid=$!
    spin_download "Đang tải ${archive_name}" 1843 "$dl_pid" "${tmp_dir}/${archive_name}"
    wait "$dl_pid" 2>/dev/null || true

    log_step "$lbl_fetch" "Tải về thành công"

    # verify checksum if checksums.txt exists in release
    local checksums_url
    checksums_url=$(echo "$releases_json" | grep "browser_download_url" | grep "checksums\.txt" | head -1 | cut -d '"' -f 4 || true)
    if [ -n "$checksums_url" ]; then
        (
            cd "$tmp_dir"
            if [ "$has_curl" -eq 1 ]; then
                curl -sLO "$checksums_url"
            else
                wget -q "$checksums_url"
            fi
        )
        if [ -f "${tmp_dir}/checksums.txt" ] && command -v sha256sum >/dev/null 2>&1; then
            local expected_hash
            expected_hash=$(grep "${archive_name}" "${tmp_dir}/checksums.txt" | awk '{print $1}')
            if [ -n "$expected_hash" ]; then
                local actual_hash
                actual_hash=$(sha256sum "${tmp_dir}/${archive_name}" | awk '{print $1}')
                if [ "$expected_hash" != "$actual_hash" ]; then
                    err "Xác thực mã băm SHA256 thất bại! File tải về có thể đã bị can thiệp."
                fi
                log_step "$lbl_check" "Xác thực SHA256 thành công (${actual_hash:0:16}...)"
            fi
        fi
    fi

    # 5. extract archive
    tar -xzf "${tmp_dir}/${archive_name}" -C "$tmp_dir"

    local lib_src="${tmp_dir}/usr/lib/fcitx5/libclak.so"
    local addon_src="${tmp_dir}/usr/share/fcitx5/addon/clak.conf"
    local im_src="${tmp_dir}/usr/share/fcitx5/inputmethod/clak.conf"
    local gui_src="${tmp_dir}/usr/bin/clak-gui"
    local desktop_src="${tmp_dir}/usr/share/applications/clak-gui.desktop"
    local icons_src="${tmp_dir}/usr/share/icons"

    if [ ! -f "$lib_src" ]; then
        err "Gói cài đặt bị lỗi: không tìm thấy file libclak.so"
    fi

    # 6. copy files (only request sudo if system install)
    local lib_dest
    local addon_dest
    local im_dest

    if [ "$target_mode" = "system" ]; then
        lib_dest="/usr/lib/fcitx5"
        addon_dest="/usr/share/fcitx5/addon"
        im_dest="/usr/share/fcitx5/inputmethod"
        print_system_sudo_notice

        run_sudo mkdir -p "$lib_dest" "$addon_dest" "$im_dest"
        run_sudo cp "$lib_src" "${lib_dest}/libclak.so"
        run_sudo cp "$addon_src" "${addon_dest}/clak.conf"
        run_sudo cp "$im_src" "${im_dest}/clak.conf"
        run_sudo chmod 755 "${lib_dest}/libclak.so"
        run_sudo chmod 644 "${addon_dest}/clak.conf" "${im_dest}/clak.conf"

        if [ -f "$gui_src" ]; then
            run_sudo mkdir -p "/usr/bin"
            run_sudo cp "$gui_src" "/usr/bin/clak-gui"
            run_sudo chmod 755 "/usr/bin/clak-gui"
        fi
        if [ -f "$desktop_src" ]; then
            run_sudo mkdir -p "/usr/share/applications"
            run_sudo cp "$desktop_src" "/usr/share/applications/clak-gui.desktop"
            run_sudo chmod 644 "/usr/share/applications/clak-gui.desktop"
            command -v update-desktop-database >/dev/null 2>&1 && run_sudo update-desktop-database "/usr/share/applications" 2>/dev/null || true
        fi
        if [ -d "$icons_src" ]; then
            run_sudo cp -r "$icons_src"/* /usr/share/icons/ 2>/dev/null || true
            if command -v gtk-update-icon-cache >/dev/null 2>&1; then
                run_sudo gtk-update-icon-cache -f -q -t "/usr/share/icons/hicolor" 2>/dev/null || true
            fi
        fi
    else
        lib_dest="${HOME}/.local/lib/fcitx5"
        addon_dest="${HOME}/.local/share/fcitx5/addon"
        im_dest="${HOME}/.local/share/fcitx5/inputmethod"

        mkdir -p "$lib_dest" "$addon_dest" "$im_dest"
        cp "$lib_src" "${lib_dest}/libclak.so"
        cp "$addon_src" "${addon_dest}/clak.conf"
        cp "$im_src" "${im_dest}/clak.conf"
        chmod 755 "${lib_dest}/libclak.so"
        chmod 644 "${addon_dest}/clak.conf" "${im_dest}/clak.conf"

        if [ -f "$gui_src" ]; then
            mkdir -p "${HOME}/.local/bin"
            cp "$gui_src" "${HOME}/.local/bin/clak-gui"
            chmod 755 "${HOME}/.local/bin/clak-gui"
        fi
        if [ -f "$desktop_src" ]; then
            mkdir -p "${HOME}/.local/share/applications"
            cp "$desktop_src" "${HOME}/.local/share/applications/clak-gui.desktop"
            chmod 644 "${HOME}/.local/share/applications/clak-gui.desktop"
            command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "${HOME}/.local/share/applications" 2>/dev/null || true
        fi
        if [ -d "$icons_src" ]; then
            mkdir -p "${HOME}/.local/share/icons"
            cp -r "$icons_src"/* "${HOME}/.local/share/icons/" 2>/dev/null || true
            if command -v gtk-update-icon-cache >/dev/null 2>&1; then
                for icondir in "${HOME}/.local/share/icons"/*; do
                    [ -d "$icondir" ] && gtk-update-icon-cache -f -q -t "$icondir" 2>/dev/null || true
                done
            fi
        fi
    fi

    log_step "$lbl_install" "Đã chép ${c_accent}${lib_dest}/libclak.so${c_reset}"
    log_step "$lbl_install" "Đã chép ${c_accent}${addon_dest}/clak.conf${c_reset}"
    log_step "$lbl_install" "Đã chép ${c_accent}${im_dest}/clak.conf${c_reset}"
    [ -f "$gui_src" ] && log_step "$lbl_install" "Đã cài đặt giao diện điều khiển cấu hình clak-gui"

    # save installed version for updater
    mkdir -p "${HOME}/.local/share/clak"
    echo "$tag_name" > "${HOME}/.local/share/clak/version"

    # 7. check and configure uinput permission automatically
    if [ -w /dev/uinput ] 2>/dev/null; then
        log_step "$lbl_uinput" "Quyền truy cập /dev/uinput đã sẵn sàng (${c_green}OK${c_reset})"
    else
        log_step "$lbl_warn" "Người dùng hiện tại chưa có quyền ghi vào /dev/uinput"
        print_uinput_sudo_notice
        if echo 'KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess"' | run_sudo tee /etc/udev/rules.d/99-uinput.rules >/dev/null 2>&1 && run_sudo udevadm trigger /dev/uinput 2>/dev/null; then
            log_step "$lbl_uinput" "Đã cấu hình /dev/uinput thành công"
        else
            log_step "$lbl_warn" "Chưa cấp quyền uinput, Clak sẽ hoạt động ở chế độ Wayland mặc định"
        fi
    fi

    # 8. check and configure wps compatibility
    configure_wps_compatibility 0

    # 9. configure autostart with system
    configure_autostart 0

    # 10. reload fcitx5 daemon with final spinner
    if command -v fcitx5 >/dev/null 2>&1; then
        (
            fcitx5 -r -d >/dev/null 2>&1 || true
            sleep 0.4
        ) &
        local reload_pid=$!
        spin_pid "$reload_pid" "Đang khởi động lại daemon Fcitx5..." 1
        wait "$reload_pid" 2>/dev/null || true
    fi

    # final line directly after spin completes
    printf "\r\033[K\n"
    echo -e "${c_green}✔ Cài đặt Clak thành công vào hệ thống Fcitx5!${c_reset}"
    echo -e "  Vào cấu hình Fcitx5 để thêm Clak vào danh sách bộ gõ"
    echo ""
}

# main router
if [ "$dry_run" -eq 1 ]; then
    run_simulation
else
    run_install
fi
