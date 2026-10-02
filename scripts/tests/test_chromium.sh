#!/usr/bin/env bash

# test typing and backspacing in chromium address bar
# author: verse & antigravity

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

if ! command -v chromium &>/dev/null; then
    echo -e "${RED}Lỗi: chromium chưa được cài đặt${NC}"
    exit 1
fi

if ! command -v wtype &>/dev/null; then
    echo -e "${RED}Lỗi: wtype chưa được cài đặt${NC}"
    exit 1
fi

TMP_PROFILE="/tmp/chromium_clak_test_$$"
rm -rf "$TMP_PROFILE"

echo -e "${CYAN}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║     TEST GÕ VÀ XÓA TRÊN ADDRESS BAR (CHROMIUM)            ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════╝${NC}"

echo -e "\n${YELLOW}1. Đang khởi chạy Chromium với profile test sạch...${NC}"
chromium --user-data-dir="$TMP_PROFILE" \
         --no-first-run \
         --no-default-browser-check \
         --disable-fre \
         "about:blank" 2>/dev/null &
CHROME_PID=$!

cleanup() {
    echo -e "\n${YELLOW}Đang đóng Chromium và dọn dẹp...${NC}"
    kill "$CHROME_PID" 2>/dev/null || true
    rm -rf "$TMP_PROFILE"
}
trap cleanup EXIT INT TERM

echo -e "${GREEN}Chromium đã mở!${NC}\n"
echo -e "${YELLOW}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${YELLOW}║ 👉 HÃY CLICK CHUỘT VÀO THANH ADDRESS BAR CỦA CHROMIUM!    ║${NC}"
echo -e "${YELLOW}╚═══════════════════════════════════════════════════════════╝${NC}"

DELAY=${1:-15}
STEP_GAP=0.35
LOG_LINE=1

show_step_logs() {
    sleep 0.05
    if [[ -f /tmp/clak.log ]]; then
        local total=$(wc -l < /tmp/clak.log)
        if (( total >= LOG_LINE )); then
            sed -n "${LOG_LINE},${total}p" /tmp/clak.log | while IFS= read -r line; do
                echo -e "   ${CYAN}↳ ${line}${NC}"
            done
            LOG_LINE=$(( total + 1 ))
        fi
    fi
}

for ((i=4; i>0; i--)); do
    echo -ne "${CYAN}Bắt đầu sau: ${i}s... Hãy click vào thanh địa chỉ của Chromium!${NC}\r"
    sleep 1
done
fcitx5-remote -s clak 2>/dev/null || true
rm -f /tmp/clak.log
LOG_LINE=1
echo -e "\n${GREEN}BẮT ĐẦU TEST TỐC ĐỘ CAO (${DELAY}ms/phím)!                    ${NC}\n"

# focus address bar automatically via ctrl+l and clear
wtype -M ctrl -k l -m ctrl
sleep 0.2
wtype -k BackSpace
sleep 0.2
show_step_logs

# scenario 1: type dd -> delete -> retype single d
echo -e "${BLUE}▶ Bước 1: Gõ 'dd' -> 'đ'${NC}"
wtype -d "$DELAY" "dd"
sleep "$STEP_GAP"
show_step_logs

echo -e "${BLUE}▶ Bước 2: BackSpace xoá 'đ'${NC}"
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs

echo -e "${BLUE}▶ Bước 3: Gõ lại 'd' (kiểm tra không bị lỗi biến thành đ)${NC}"
wtype -d "$DELAY" "d"
sleep "$STEP_GAP"
show_step_logs

echo -e "${BLUE}▶ Bước 4: Gõ tiếp 'd' thứ 2 -> biến thành 'đ'${NC}"
wtype -d "$DELAY" "d"
sleep "$STEP_GAP"
show_step_logs

echo -e "${BLUE}▶ Bước 5: Xoá 'đ'${NC}"
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs

# scenario 2: type aa -> delete -> retype a
echo -e "${BLUE}▶ Bước 6: Gõ 'aa' -> 'â', xoá, gõ lại 'a' rồi 'a' -> 'â'${NC}"
wtype -d "$DELAY" "aa"
sleep "$STEP_GAP"
show_step_logs
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs
wtype -d "$DELAY" "a"
sleep "$STEP_GAP"
show_step_logs
wtype -d "$DELAY" "a"
sleep "$STEP_GAP"
show_step_logs
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs

# scenario 3: type oo -> delete -> retype o
echo -e "${BLUE}▶ Bước 7: Gõ 'oo' -> 'ô', xoá, gõ lại 'o' rồi 'o' -> 'ô'${NC}"
wtype -d "$DELAY" "oo"
sleep "$STEP_GAP"
show_step_logs
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs
wtype -d "$DELAY" "o"
sleep "$STEP_GAP"
show_step_logs
wtype -d "$DELAY" "o"
sleep "$STEP_GAP"
show_step_logs
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs

# scenario 4: type ee -> delete -> retype e
echo -e "${BLUE}▶ Bước 8: Gõ 'ee' -> 'ê', xoá, gõ lại 'e' rồi 'e' -> 'ê'${NC}"
wtype -d "$DELAY" "ee"
sleep "$STEP_GAP"
show_step_logs
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs
wtype -d "$DELAY" "e"
sleep "$STEP_GAP"
show_step_logs
wtype -d "$DELAY" "e"
sleep "$STEP_GAP"
show_step_logs
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs

# scenario 5: type ddaay -> backspace 2 times to đ -> type oo -> đô
echo -e "${BLUE}▶ Bước 9: Gõ 'ddaay' ('đây'), xoá 2 lần về 'đ', gõ 'oo' thành 'đô'${NC}"
wtype -d "$DELAY" "ddaay"
sleep "$STEP_GAP"
show_step_logs
wtype -k BackSpace
sleep 0.1
wtype -k BackSpace
sleep "$STEP_GAP"
show_step_logs
wtype -d "$DELAY" "oo"
sleep "$STEP_GAP"
show_step_logs

# scenario 6: clear and blast full sentences fast
echo -e "${BLUE}▶ Bước 10: Xóa hết và bắn câu dài siêu tốc (${DELAY}ms/phím)${NC}"
for ((k=0; k<15; k++)); do
    wtype -k BackSpace
    sleep 0.02
done
sleep 0.3
wtype -d "$DELAY" "ddaay laf booj gox tieengs vieetj clak treen address bar"
sleep 1.0
show_step_logs

echo -e "\n${GREEN}✔ Hoàn tất toàn bộ kịch bản test tốc độ cao!${NC}"
echo -e "${YELLOW}Cửa sổ Chromium vẫn đang mở để bạn quan sát hoặc gõ tay thử nghiệm.${NC}"
read -p "Nhấn [Enter] để đóng Chromium và kết thúc: "
