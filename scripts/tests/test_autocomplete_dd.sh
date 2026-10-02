#!/usr/bin/env bash

# test typing dd -> đ with autocomplete suggestion (like douyin.com)
# author: verse & antigravity

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

TMP_PROFILE="/tmp/chromium_autoc_test_$$"
rm -rf "$TMP_PROFILE"

echo -e "${CYAN}Đang khởi chạy Chromium...${NC}"
chromium --user-data-dir="$TMP_PROFILE" \
         --no-first-run \
         --no-default-browser-check \
         --disable-fre \
         "about:blank" 2>/dev/null &
CHROME_PID=$!

cleanup() {
    kill "$CHROME_PID" 2>/dev/null || true
    rm -rf "$TMP_PROFILE"
}
trap cleanup EXIT INT TERM

sleep 1.2
fcitx5-remote -s clak 2>/dev/null || true

# seed history with douyin.com so it autocompletes on 'd'
echo -e "${YELLOW}Tạo lịch sử douyin.com trong omnibox...${NC}"
wtype -M ctrl -k l -m ctrl
sleep 0.2
wtype -d 15 "douyin.com"
sleep 0.2
wtype -k Return
sleep 0.8

rm -f /tmp/clak.log
LOG_LINE=1

show_step_logs() {
    sleep 0.1
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

echo -e "\n${BLUE}=== FOCUS ADDRESS BAR & CLEAR ===${NC}"
wtype -M ctrl -k l -m ctrl
sleep 0.2
wtype -k BackSpace
sleep 0.2
show_step_logs

echo -e "\n${BLUE}▶ 1. Gõ 'd' (xem có autocomplete douyin.com không)${NC}"
wtype -d 20 "d"
sleep 0.4
show_step_logs

echo -e "\n${BLUE}▶ 2. Gõ 'd' thứ hai (kiểm tra đ hay dđ)${NC}"
wtype -d 20 "d"
sleep 0.4
show_step_logs

echo -e "\n${BLUE}▶ 3. Xoá 'đ'${NC}"
wtype -k BackSpace
sleep 0.3
show_step_logs

echo -e "\n${BLUE}▶ 4. Gõ lại 'dd' siêu nhanh (10ms)${NC}"
wtype -d 10 "dd"
sleep 0.4
show_step_logs

echo -e "\n${BLUE}▶ 5. Xoá và gõ 'dddddd' siêu nhanh (10ms)${NC}"
wtype -k BackSpace
sleep 0.2
wtype -d 10 "dddddd"
sleep 0.4
show_step_logs

echo -e "\n${GREEN}✔ Hoàn tất test autocomplete!${NC}"
