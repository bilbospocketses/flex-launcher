# Items 18 and 19: titles scale with the button by default, stop at a readable minimum, and a
# fixed FontSize or Padding still means what it did

# A dense 8 x 4 grid: 14% of these buttons would be unreadable, so titles stop at 22 pt (2% of 1080)
run_quick f40-dense
ok=1
grep -qE "Menu 'Main': 8 x 4 grid, [0-9]+ px buttons, 22 pt titles" "$out/f40-dense.log" \
    && sanitizer_clean f40-dense && ok=0
result "items 18-19: a dense grid's titles stop at the readable minimum" $ok
grep "Menu 'Main':" "$out/f40-dense.log" | sed 's/^/      /'

# Three large buttons: the titles grow with them (round(556 * 0.14) = 78)
run_quick f40-large
ok=1
grep -q "Menu 'Main': 3 x 1 grid, 556 px buttons, 78 pt titles" "$out/f40-large.log" \
    && sanitizer_clean f40-large && ok=0
result "item 19: large buttons get titles to match" $ok
grep "Menu 'Main':" "$out/f40-large.log" | sed 's/^/      /'

# FontSize=36 and Padding=20 keep today's fixed titles
run_quick f40-fixed
ok=1
grep -q "Menu 'Main': 4 x 1 grid, 256 px buttons, 36 pt titles" "$out/f40-fixed.log" \
    && grep -A10 'Titles ===' "$out/f40-fixed.log" | grep -qE 'FontSize:\s+36$' \
    && grep -A10 'Titles ===' "$out/f40-fixed.log" | grep -qE '^Padding:\s+20$' \
    && sanitizer_clean f40-fixed && ok=0
result "items 18-19: a fixed FontSize and Padding still mean what they did" $ok

# Titles off: nothing under the buttons, and no font sized for them
run_quick f40-notitles
ok=1
[ "$(cat "$out/f40-notitles.code")" = 0 ] && grep -q "Menu 'Main': 4 x 1 grid, [0-9]* px buttons, no titles" "$out/f40-notitles.log" \
    && sanitizer_clean f40-notitles && ok=0
result "titles turned off with a percentage FontSize" $ok

# OversizeMode=Truncate (the documented spelling) parses, and so does the old Truncated
for f in f40-truncate f40-truncated; do
    run_quick $f
    ok=1
    grep -A10 'Titles ===' "$out/$f.log" | grep -qE 'OversizeMode:\s+Truncate$' && sanitizer_clean $f && ok=0
    result "OversizeMode $(grep -o 'OversizeMode=[A-Za-z]*' "$FX/$f.ini") parses as Truncate" $ok
done

# Shrink mode on long titles in a dense grid: the titles are already at the 22 pt minimum, so
# Shrink cuts them without opening a smaller font, and without a crash
run_quick f40-shrink
ok=1
[ "$(cat "$out/f40-shrink.code")" = 0 ] \
    && grep -qE "Menu 'Main': 8 x 4 grid, [0-9]+ px buttons, 22 pt titles" "$out/f40-shrink.log" \
    && sanitizer_clean f40-shrink && ok=0
result "item 19: Shrink mode on long titles in a dense grid" $ok

# Shrink mode under large buttons: at 78 pt every title here is wider than its 556 px button, so
# render_text opens smaller fonts. One title fits at its first smaller size, one needs a step down
# (a font closed and another opened), and one stops at the 22 pt minimum and is cut. The harness
# runs with detect_leaks=0, so this finds a use-after-free or a double close there, not a leak.
run_quick f40-shrink-large
ok=1
[ "$(cat "$out/f40-shrink-large.code")" = 0 ] \
    && grep -q "Menu 'Main': 3 x 1 grid, 556 px buttons, 78 pt titles" "$out/f40-shrink-large.log" \
    && sanitizer_clean f40-shrink-large && ok=0
result "item 19: Shrink mode on long titles under large buttons steps down and stops at the minimum" $ok

# A FontSize that is neither a size nor a percentage is refused with a log line
run_quick f40-junk
ok=1
grep -q "Invalid FontSize value '12pt'" "$out/f40-junk.log" && grep -q "Invalid Padding value '20px'" "$out/f40-junk.log" \
    && sanitizer_clean f40-junk && ok=0
result "FontSize=12pt and Padding=20px are refused with a log line" $ok
