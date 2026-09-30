# One parse path (3b): the table reads every key, Config keeps what the file says, and the
# values the launcher draws with are worked out from it. The spec's three parser bugs are fixed.

# A negative [Clock] FontSize is refused with a log line, and the clock still shows at its default
run_quick f41-clocksize
ok=1
grep -q "Invalid FontSize value '-5' in \[Clock\], ignoring it" "$out/f41-clocksize.log" \
    && grep -A12 'Clock ===' "$out/f41-clocksize.log" | grep -qE '^FontSize:\s+50$' \
    && ran_clean f41-clocksize && ok=0
result "a negative clock FontSize is refused with a log line (exit $(cat "$out/f41-clocksize.code"))" $ok

# :exit on Linux, as a hotkey (F2) or an entry's command, says it works only on Windows
run_keys f41-exit F2 Return
ok=1
[ "$(grep -c "':exit' works only as a hotkey on Windows" "$out/f41-exit.log")" = 2 ] && ran_clean f41-exit && ok=0
result ":exit outside a Windows hotkey says why it does nothing (exit $(cat "$out/f41-exit.code"))" $ok

# A FontFace the file does not have falls back to the bundled font, and the setting stays as written
run_quick f41-fontface
ok=1
grep -q 'Could not open the font /work/build/assets/fonts/DejaVuSans.ttf (face 9), using the default font' "$out/f41-fontface.log" \
    && grep -A14 'Titles ===' "$out/f41-fontface.log" | grep -qE '^FontFace:\s+9$' \
    && grep -A14 'Titles ===' "$out/f41-fontface.log" | grep -qE '^Font:\s+/work/build/assets/fonts/DejaVuSans.ttf$' \
    && ran_clean f41-fontface && ok=0
result "a FontFace the font does not have falls back and stays as written (exit $(cat "$out/f41-fontface.code"))" $ok

# Values in forms the table never read before: kept as written in Config, and the effective ones
# worked out from them (12.5% of 1080 is 135 px, clamped to the 25% minimum, 270; HPadding 300 is
# kept to half the 40 px gap)
run_quick f41-values
ok=1
log=$out/f41-values.log
! grep -q 'Invalid' "$log" \
    && grep -A6 'General ===' "$log" | grep -qE '^FPSLimit:\s+10$' \
    && grep -A6 'Layout ===' "$log" | grep -qE '^IconSpacing:\s+40$' \
    && grep -A6 'Layout ===' "$log" | grep -qE '^VCenter:\s+12.5%$' \
    && grep -q 'Effective: IconSpacing 40 px, VCenter 270 px, HPadding 20 px' "$log" \
    && ran_clean f41-values && ok=0
result "values in new forms are kept as written, and the drawn ones worked out from them (exit $(cat "$out/f41-values.code"))" $ok
grep -E '^(FPSLimit|IconSpacing|VCenter|Effective):' "$log" | sed 's/^/      /'
