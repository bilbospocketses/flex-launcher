# Shadowed titles: a title's shadow is drawn 1/40 of its line height below it, at least 2 px (2 px
# for a 36 pt title), and the layout keeps that room under every title. A height-limited grid of
# 4 rows so gives each of its buttons 2 px, the same config without shadows having none to give.

# A function to read the button size from a run's "1 x 4 grid" line
grid_px() { grep -m1 -oE "Menu 'Main': 1 x 4 grid, [0-9]+ px buttons" "$out/$1.log" | grep -oE '[0-9]+ px' | grep -oE '[0-9]+'; }

run_quick f45-noshadow
run_quick f45-shadow
plain=$(grid_px f45-noshadow)
shadowed=$(grid_px f45-shadow)
ok=1
ran_clean f45-noshadow && ran_clean f45-shadow && [ -n "$plain" ] && [ "$shadowed" = "$((plain - 2))" ] && ok=0
result "shadowed titles keep room for their shadows: 4 rows of ${plain:-?} px buttons become ${shadowed:-?} px (exit $(cat "$out/f45-shadow.code"))" $ok

# Titles too long for their buttons, shadowed: each is cut to its button less the shadow's reach,
# so title and shadow together fit the button. The debug log names any title (measured with its
# shadow) wider than its button.
run_quick f45-shadowlong
ok=1
ran_clean f45-shadowlong && grep -qE "Menu 'Main': 4 x 1 grid, [0-9]+ px buttons, 36 pt titles" "$out/f45-shadowlong.log" \
    && ! grep -q 'px wide, over its' "$out/f45-shadowlong.log" && ok=0
result "a shadowed title cut to fit keeps its shadow inside its button (exit $(cat "$out/f45-shadowlong.code"))" $ok
grep 'px wide, over its' "$out/f45-shadowlong.log" | sed 's/^/      /'
