# How menu sections are read. A section can appear twice with another menu between (configs
# assembled from pieces do this), and an entry can be left empty.

# The second [Main] block's entry and Rows belong to Main, not to Games, the menu read before it
run_quick f25-split
ok=1
grep -A2 'Menu Name: Main' "$out/f25-split.log" | grep -qE 'Number of Entries: 2$' \
    && grep -A2 'Menu Name: Main' "$out/f25-split.log" | grep -qE 'Rows 2, ' \
    && grep -A2 'Menu Name: Games' "$out/f25-split.log" | grep -qE 'Number of Entries: 1$' \
    && grep -A2 'Menu Name: Games' "$out/f25-split.log" | grep -qE 'Rows 0, ' \
    && sanitizer_clean f25-split && ok=0
result "a menu section that appears twice keeps its own entries and grid" $ok

# Entry2= is skipped with a log line; the entries either side of it stay
run_quick f25-empty
ok=1
[ "$(cat "$out/f25-empty.code")" = 0 ] && grep -q "Menu 'Main': 'Entry2' is empty, ignoring it" "$out/f25-empty.log" \
    && grep -A1 'Menu Name: Main' "$out/f25-empty.log" | grep -qE 'Number of Entries: 2$' \
    && sanitizer_clean f25-empty && ok=0
result "an empty entry is skipped instead of crashing (exit $(cat "$out/f25-empty.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f25-empty.err" | sed 's/^/      /'
