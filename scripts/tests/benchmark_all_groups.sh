#!/usr/bin/env bash
set -e

# natural typing benchmark at ~80ms per keystroke (approx 60-70 WPM)
DELAY=${1:-80}
CONVERSATION="ddaay laf heej thoongs goox tieengs vieetj clak. chungws ta ddang thuwr nghieemj ddooj muowjt vaf ddooj treex cuar tuwngf nhooms uwngs dujng. mooxij nguwowif ddeefu mong muooxn cos mootj booj gox nhanh, chiwnh xasc vaf khoong bij lag. hoom nay trowif ddejp, chowf chungws ta cungf nhau uoongs caf phee vaf trof chuyeejn vui ver. coong vieecj lajp trifnh ddaatj nhieeuf keest quar toots ddepj."

echo "=== BENCHMARK CLAK: TỐC ĐỘ GÕ TỰ NHIÊN (${DELAY}ms / phím) ==="

# 1. Terminal-Uinput
echo -e "\n[1/5] Testing Terminal-Uinput (Kitty)..."
rm -f /tmp/term_out.txt
kitty --title "bench_kitty" bash -c "sleep 0.2; cat > /tmp/term_out.txt" &
TERM_PID=$!
sleep 1.2
fcitx5-remote -s clak || true
wtype -d "$DELAY" "$CONVERSATION"
sleep 0.5
wtype -k Return
sleep 0.5
kill $TERM_PID 2>/dev/null || true
wait $TERM_PID 2>/dev/null || true

# 2. Chromium-SurroundingText
echo -e "\n[2/5] Testing Chromium-SurroundingText..."
chromium --ozone-platform=wayland --enable-wayland-ime /tmp/test_chromium_surr.html &
CHROM_PID=$!
sleep 2.0
fcitx5-remote -s clak || true
wtype -d "$DELAY" "$CONVERSATION"
sleep 0.5
kill $CHROM_PID 2>/dev/null || true
wait $CHROM_PID 2>/dev/null || true

# 3. Docs-Uinput
echo -e "\n[3/5] Testing Docs-Uinput..."
chromium --ozone-platform=wayland --enable-wayland-ime /tmp/test_docs.html &
DOCS_PID=$!
sleep 2.0
fcitx5-remote -s clak || true
wtype -d "$DELAY" "$CONVERSATION"
sleep 0.5
kill $DOCS_PID 2>/dev/null || true
wait $DOCS_PID 2>/dev/null || true

# 4. Gecko-Uinput (Zen Browser / Firefox)
echo -e "\n[4/5] Testing Gecko-Uinput..."
GECKO_BIN="zen-browser"
if ! command -v "$GECKO_BIN" &>/dev/null; then
    GECKO_BIN="firefox"
fi
"$GECKO_BIN" /tmp/test_chromium_surr.html &
GECKO_PID=$!
sleep 3.0
fcitx5-remote -s clak || true
wtype -d "$DELAY" "$CONVERSATION"
sleep 0.5
kill $GECKO_PID 2>/dev/null || true
wait $GECKO_PID 2>/dev/null || true

# 5. address-bar
echo -e "\n[5/5] Testing address-bar..."
chromium --ozone-platform=wayland --enable-wayland-ime "about:blank" &
ADDR_PID=$!
sleep 2.0
fcitx5-remote -s clak || true
# focus address bar
wtype -M ctrl -k l -m ctrl
sleep 0.5
ADDR_TEXT="ddantri.com.vn tieengs vieetj haam mooj ddoongf ddooj chuyeern doonjng phat trirn ddaat nuowsc tieengs vieetj clak booj gox thoongs nhaast thaafnh phoose ddaay laf phaanf meefm toots"
wtype -d "$DELAY" "$ADDR_TEXT"
sleep 0.5
kill $ADDR_PID 2>/dev/null || true
wait $ADDR_PID 2>/dev/null || true

echo -e "\n=== KẾT QUẢ THỐNG KÊ LATENCY ==="
python3 scripts/tests/analyze_latency.py /tmp/clak.log
