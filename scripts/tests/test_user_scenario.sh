#!/usr/bin/env bash

# test single scenario: dd -> del -> dddddd -> del -> dd -> đ
# author: verse & antigravity

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

TMP_PROFILE="/tmp/chromium_user_test_$$"
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
rm -f /tmp/clak.log
LOG_LINE=1

show_step_logs() {
    sleep 0.08
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

# focus address bar and select all, then clear
wtype -M ctrl -k l -m ctrl
sleep 0.2
wtype -M ctrl -k a -m ctrl
sleep 0.1
wtype -k BackSpace
sleep 0.2
show_step_logs

echo -e "\n${BLUE}▶ 1. Gõ 'dd' -> 'đ' (5ms/phím)${NC}"
wtype -d 5 "dd"
sleep 0.25
show_step_logs

echo -e "\n${BLUE}▶ 2. Xoá 'đ' (BackSpace)${NC}"
wtype -k BackSpace
sleep 0.25
show_step_logs

echo -e "\n${BLUE}▶ 3. Gõ 'dddddd' (6 chữ 'd', 5ms/phím)${NC}"
wtype -d 5 "dddddd"
sleep 0.25
show_step_logs

echo -e "\n${BLUE}▶ 4. Xoá sạch (6 lần BackSpace với delay 30ms)${NC}"
wtype -k BackSpace -s 30 -k BackSpace -s 30 -k BackSpace -s 30 -k BackSpace -s 30 -k BackSpace -s 30 -k BackSpace
sleep 0.35
show_step_logs

echo -e "\n${BLUE}▶ 5. Gõ lại 'dd' -> 'đ' (5ms/phím)${NC}"
wtype -d 5 "dd"
sleep 0.25
show_step_logs

echo -e "\n${GREEN}✔ Hoàn thành test kịch bản!${NC}"
