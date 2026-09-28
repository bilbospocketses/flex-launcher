# Item 22: Xvfb reports a refresh rate of 0, as some VMs and remote desktops do. The launcher
# must start, use 60 Hz and say so, instead of dividing by zero.
run_quick f22-refresh
ok=1
ran_clean f22-refresh && grep -q 'The display reports no refresh rate, using 60 Hz' "$out/f22-refresh.log" && ok=0
result "item 22: a display that reports 0 Hz starts at 60 Hz (exit $(cat "$out/f22-refresh.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f22-refresh.err" | sed 's/^/      /'
