# Each feature starts, stops and frees what it holds; the frame timing is worked out from VSync and
# FPSLimit without changing either. The leak pass (run.sh leaks) proves the stops free everything.

# FPSLimit=10 is the documented minimum: with VSync off it sets the frame time, and VSync stays off
run_quick f42-fps10
ok=1
grep -q 'Frame timing: FPS limit 10, 100 ms a frame' "$out/f42-fps10.log" \
    && grep -q 'Frame timing: VSync wanted off, the renderer gives off' "$out/f42-fps10.log" \
    && grep -A4 'General ===' "$out/f42-fps10.log" | grep -qE '^VSync:\s+false$' \
    && ran_clean f42-fps10 && ok=0
result "FPSLimit=10 with VSync off sets 100 ms frames, and leaves VSync as written (exit $(cat "$out/f42-fps10.code"))" $ok

# A renderer that refuses VSync (a test hook stands in for one: the harness's SDL simulates VSync)
# is paced by the frame delay instead, and the refusal is logged once
STREAMFLEX_TEST_VSYNC_REFUSED=1 run_quick f42-vsync
ok=1
grep -q 'Frame timing: VSync wanted on, the renderer gives off' "$out/f42-vsync.log" \
    && [ "$(grep -c 'The renderer refused VSync' "$out/f42-vsync.log")" = 1 ] \
    && ran_clean f42-vsync && ok=0
result "a renderer that refuses VSync is logged once, and VSync counts as off (exit $(cat "$out/f42-vsync.code"))" $ok
grep 'VSync' "$out/f42-vsync.log" | sed 's/^/      /'

# Every feature running at once, the screensaver on and off again, the virtual pad attached, and a
# clean quit: every feature's stop runs at quit
wake() { xdotool key Right; sleep 1; }
rm -f /tmp/pad-none
STREAMFLEX_TEST_PAD=/tmp/pad-none UNTIL='Screensaver off' run_after_line f42-all 'Screensaver on' +wake
log=$out/f42-all.log
stops=$(sed -n '/Quitting program/,$p' "$log")
all_stopped=0
for feature in Clock Screensaver 'Scroll indicators' Highlight Overlay Gamepad; do
    grep -q "^$feature stopped" <<< "$stops" || { all_stopped=1; echo "      no '$feature stopped' after 'Quitting program'"; }
done
ok=1
grep -q 'Overlay started' "$log" && grep -q 'Highlight started' "$log" && grep -q 'Scroll indicators started' "$log" \
    && grep -q 'Clock started' "$log" && grep -q 'Screensaver started' "$log" && grep -q 'Gamepad started' "$log" \
    && grep -q 'Gamepad connected with device index 0' "$log" \
    && grep -q 'Frame timing: VSync wanted on, the renderer gives on' "$log" \
    && [ $all_stopped = 0 ] && ran_clean f42-all && ok=0
result "every feature starts, and all six stop at quit (exit $(cat "$out/f42-all.code"))" $ok
grep -E '(started|stopped|Screensaver o)' "$log" | sed 's/^/      /'

# The virtual pad is attached as the gamepad starts, before it lists the pads present, as a pad
# plugged in before the launcher is: the listing connects it, and its connect event is still
# queued. It is connected, and opened, once.
connects=$(grep -c 'Gamepad connected with device index 0' "$log")
mappings=$(grep -c 'Gamepad Mapping:' "$log")
ok=1
[ "$connects" = 1 ] && [ "$mappings" = 1 ] && ran_clean f42-all && ok=0
result "a pad present when the gamepad starts is connected and opened once (connected $connects times, opened $mappings times)" $ok

# A pad whose device index is not its instance id: attach A, attach B, detach A, attach C, so C
# arrives at B's old device index with an instance id of its own. It is added, not taken for B.
settle() { sleep 1; }
STREAMFLEX_TEST_PAD_SWAP=1 run_after_line f42-padswap 'Test hook: pad C attached' +settle
log=$out/f42-padswap.log
c=$(grep -o 'Test hook: pad C attached at device index [0-9]*, instance id [0-9]*' "$log" | head -1)
c_index=$(sed 's/.*device index \([0-9]*\),.*/\1/' <<< "$c")
c_id=${c##* }
ok=1
[ -n "$c" ] && [ "$c_index" != "$c_id" ] && grep -q 'Gamepad disconnected' "$log" \
    && grep -q "Gamepad connected with device index $c_index, instance id $c_id" "$log" \
    && ran_clean f42-padswap && ok=0
result "a pad at another pad's old device index is added by its instance id (exit $(cat "$out/f42-padswap.code"))" $ok
grep -E 'Test hook: pad|Gamepad (connected|disconnected)' "$log" | sed 's/^/      /'

# The clock's render thread fails to start (a test hook; the delay hook makes it render every
# second): each render falls back to the main thread, so the clock keeps rendering
let_it_render() { sleep 4; }
STREAMFLEX_TEST_CLOCK_THREAD_FAIL=1 STREAMFLEX_TEST_CLOCK_DELAY_MS=0 CFG=$FX/f42-all.ini run_keys f42-clockfail +let_it_render
fallbacks=$(grep -c "Could not start the clock's render thread" "$out/f42-clockfail.log")
ok=1
[ "$fallbacks" -ge 2 ] && ran_clean f42-clockfail && ok=0
result "a clock thread that fails to start renders on the main thread, and again next time ($fallbacks fallbacks; exit $(cat "$out/f42-clockfail.code"))" $ok
