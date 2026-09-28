# How menu sections are read. A section can appear twice with another menu between (configs
# assembled from pieces do this), and an entry can be left empty.

# The second [Main] block's entry and Rows belong to Main, not to Games, the menu read before it
run_quick f25-split
ok=1
grep -A2 'Menu Name: Main' "$out/f25-split.log" | grep -qE 'Number of Entries: 2$' \
    && grep -A2 'Menu Name: Main' "$out/f25-split.log" | grep -qE 'Rows 2, ' \
    && grep -A2 'Menu Name: Games' "$out/f25-split.log" | grep -qE 'Number of Entries: 1$' \
    && grep -A2 'Menu Name: Games' "$out/f25-split.log" | grep -qE 'Rows 0, ' \
    && ran_clean f25-split && ok=0
result "a menu section that appears twice keeps its own entries and grid (exit $(cat "$out/f25-split.code"))" $ok

# Entry2= is skipped with a log line; the entries either side of it stay. Entry4 (no command)
# and Entry5 (:select) are refused, which the leak pass needs: a refused entry frees its strings.
run_quick f25-empty
ok=1
ran_clean f25-empty && grep -q "Menu 'Main': 'Entry2' is empty, ignoring it" "$out/f25-empty.log" \
    && grep -A1 'Menu Name: Main' "$out/f25-empty.log" | grep -qE 'Number of Entries: 2$' && ok=0
result "an empty entry is skipped instead of crashing, and refused ones are dropped (exit $(cat "$out/f25-empty.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f25-empty.err" | sed 's/^/      /'
