# Each feature starts, stops and frees what it holds; the frame timing is worked out from VSync and
# FPSLimit without changing either. The leak pass (run.sh leaks) proves the stops free everything.

# FPSLimit=10 is the documented minimum: with VSync off it sets the frame time, and VSync stays off
run_quick f42-fps10
ok=1
grep -q 'Frame timing: FPS limit 10, 100 ms a frame' "$out/f42-fps10.log" \
    && grep -A4 'General ===' "$out/f42-fps10.log" | grep -qE '^VSync:\s+false$' \
    && ran_clean f42-fps10 && ok=0
result "FPSLimit=10 with VSync off sets 100 ms frames, and leaves VSync as written (exit $(cat "$out/f42-fps10.code"))" $ok

# Every feature running at once, the screensaver on and off again, the virtual pad attached, and a
# clean quit: every feature's stop runs at quit
wake() { xdotool key Right; sleep 1; }
rm -f /tmp/pad-none
STREAMFLEX_TEST_PAD=/tmp/pad-none UNTIL='Screensaver off' run_after_line f42-all 'Screensaver on' +wake
log=$out/f42-all.log
ok=1
grep -q 'Overlay started' "$log" && grep -q 'Highlight started' "$log" && grep -q 'Scroll indicators started' "$log" \
    && grep -q 'Clock started' "$log" && grep -q 'Screensaver started' "$log" && grep -q 'Gamepad started' "$log" \
    && grep -q 'Gamepad connected with device index 0' "$log" \
    && sed -n '/Quitting program/,$p' "$log" | grep -q 'Clock stopped' \
    && sed -n '/Quitting program/,$p' "$log" | grep -q 'Gamepad stopped' \
    && ran_clean f42-all && ok=0
result "every feature starts, and stops at quit (exit $(cat "$out/f42-all.code"))" $ok
grep -E '(started|stopped|Screensaver o)' "$log" | sed 's/^/      /'
