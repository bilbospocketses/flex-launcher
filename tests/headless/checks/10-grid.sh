# Items 12-17: the grid review's follow-ups (#32)

# Item 12: no HPadding/VPadding in the config means the documented 30, and the outline keeps its size
run_quick f12-padding
hl=$(grep -A8 'Highlight ===' "$out/f12-padding.log")
ok=1
echo "$hl" | grep -qE 'VPadding:\s+30$' && echo "$hl" | grep -qE 'HPadding:\s+30$' \
    && echo "$hl" | grep -qE 'OutlineSize:\s+5$' && ran_clean f12-padding && ok=0
result "item 12: unset padding is 30 and OutlineSize stays 5 (exit $(cat "$out/f12-padding.code"))" $ok

# Item 13: Main -> Games -> Games (opened from itself) -> Back must return to Main
run_keys f13-selfsub Return Return BackSpace
last=$(grep -o "Loading menu '[^']*'" "$out/f13-selfsub.log" | tail -1)
ok=1
[ "$last" = "Loading menu 'Main'" ] && [ "$(grep -c "Loading menu 'Games'" "$out/f13-selfsub.log")" = 2 ] \
    && ran_clean f13-selfsub && ok=0
result "item 13: Back after a menu opens itself returns to Main (exit $(cat "$out/f13-selfsub.code"))" $ok

# Item 14: junk MaxButtons and IconSize are logged and ignored
run_quick f14-junk
ok=1
grep -q "Invalid MaxButtons value '7x'" "$out/f14-junk.log" \
    && grep -q "Invalid IconSize value '200px'" "$out/f14-junk.log" \
    && grep -A3 'Layout ===' "$out/f14-junk.log" | grep -qE 'Columns:\s+4$' \
    && grep -q "Invalid IconSize value '2000' in menu 'Main'" "$out/f14-junk.log" \
    && ran_clean f14-junk && ok=0
result "item 14: MaxButtons=7x and IconSize=200px are rejected with a log line (exit $(cat "$out/f14-junk.code"))" $ok

# Item 15: huge Rows, Columns and IconSpacing stay inside int and the grid still comes out
run_quick f15-limits
ok=1
ran_clean f15-limits && grep -q "Menu 'Main': [0-9]* x [0-9]* grid" "$out/f15-limits.log" && ok=0
result "item 15: Rows/Columns=999999 and IconSpacing=2000000000 lay out without overflow (exit $(cat "$out/f15-limits.code"))" $ok

# Item 16: the debug log shows the grid of a menu that is never opened
run_quick f16-debuglayout
ok=1
grep -A4 'Menu Name: Games' "$out/f16-debuglayout.log" | grep -qE 'Layout: 6 x 3 grid, [0-9]+ px buttons' \
    && ran_clean f16-debuglayout && ok=0
result "item 16: the debug log shows Games' 6 x 3 grid without opening it (exit $(cat "$out/f16-debuglayout.code"))" $ok

# Item 17: a reduced grid is reported once, though the menu loads twice (at startup, and again
# from the StartupCmd, which has run once the program loop begins)
UNTIL='Begin program loop' run_keys f17-once
n=$(grep -c "Menu 'Big': not enough screen space" "$out/f17-once.err")
ok=1
[ "$n" = 1 ] && [ "$(grep -c "Loading menu 'Big'" "$out/f17-once.log")" = 2 ] && ran_clean f17-once && ok=0
result "item 17: the reduction is logged once across two loads (logged $n times, exit $(cat "$out/f17-once.code"))" $ok
