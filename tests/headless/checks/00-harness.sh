# The harness's own helpers (run.sh), checked before anything relies on them: a launcher that
# starts slowly still gets its keys, one that ignores TERM is killed and fails its check, one
# that exits without logging does not hold the run up, and a log range must be closed to count

# Stand-in launchers, run as the test user as the real one is
mkdir -p /tmp/harness
cat > /tmp/harness/slow-start << EOF
#!/bin/sh
sleep 6
exec /work/build/streamflex "\$@"
EOF
cat > /tmp/harness/ignores-term << EOF
#!/bin/sh
mkdir -p "$(dirname "$LOG")"
echo "Loading menu 'Stand-in'" >> "$LOG"
trap '' TERM
exec sleep 60
EOF
printf '#!/bin/sh\nexit 0\n' > /tmp/harness/exits-at-once
chmod 755 /tmp/harness/*

# A launcher that takes 6 s to start: the keys wait for its first menu, so Back still comes
# after Games has opened twice (the keys of item 13 in 10-grid.sh)
CFG=$FX/f13-selfsub.ini exe=/tmp/harness/slow-start run_keys h-slow Return Return BackSpace
ok=1
[ "$(grep -o "Loading menu '[^']*'" "$out/h-slow.log" | tail -1)" = "Loading menu 'Main'" ] \
    && [ "$(grep -c "Loading menu 'Games'" "$out/h-slow.log")" = 2 ] && ran_clean h-slow && ok=0
result "harness: a launcher that starts 6 s late still gets its keys (exit $(cat "$out/h-slow.code"))" $ok

# A launcher that ignores TERM is killed 10 s later (exit 137), and its check fails on the exit
# code alone: its sanitizers are quiet
CFG=none exe=/tmp/harness/ignores-term run_keys h-kill
ok=1
[ "$(cat "$out/h-kill.code")" = 137 ] && sanitizer_clean h-kill && ! ran_clean h-kill && ok=0
result "harness: a launcher that ignores TERM is killed, and fails its check (exit $(cat "$out/h-kill.code"))" $ok

# A launcher that exits without logging its first menu: the wait ends when it does, not 20 s
# later, and the missing line fails the check though it exited 0
SECONDS=0
CFG=none exe=/tmp/harness/exits-at-once run_keys h-gone
took=$SECONDS
ok=1
[ "$took" -lt 5 ] && ! ran_clean h-gone && grep -q "never logged 'Loading menu'" "$out/h-gone.code" && ok=0
result "harness: a launcher that exits without logging ends the wait at once, and fails (exit $(cat "$out/h-gone.code"))" $ok
echo "      the wait took $took s"

# A log range counts only once its end line comes: a launcher that stopped logging inside
# settings cannot pass a check on the lines inside them
printf 'Settings opened\nScreensaver off\n' > "$out/h-range-open.log"
printf 'Settings opened\nScreensaver off\nSettings closed\n' > "$out/h-range-closed.log"
ok=1
! in_range "$out/h-range-open.log" 'Settings opened' 'Settings closed' 'Screensaver off' \
    && in_range "$out/h-range-closed.log" 'Settings opened' 'Settings closed' 'Screensaver off' && ok=0
result "harness: a log range with no end line does not count" $ok
