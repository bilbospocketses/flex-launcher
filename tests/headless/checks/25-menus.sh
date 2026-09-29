# How menu sections are read. A section can appear twice with another menu between (configs
# assembled from pieces do this), and an entry can be left empty. First, a config that cannot be
# opened at all.

# A function for a +key: close the message box a fatal error shows, when the SDL in use shows one
# (SDL3's does; Debian's SDL2 returns at once). Return is its default button. The window can be
# found before it listens for keys, so Return is sent until the launcher has gone, for up to 10 s.
close_message_box() {
    local i id
    for i in $(seq 50); do
        running "$2" || return 0
        id=$(xdotool search --name '^StreamFlex$' 2> /dev/null | head -1)
        # xdotool warns that the display has no focused window (there is no window manager)
        [ -z "$id" ] || xdotool key --window "$id" Return > /dev/null 2>&1
        sleep 0.2
    done
}

# A config the launcher finds but cannot open (a socket passes the readable test, and open() then
# fails) stops it with the reason, not only the fact
rm -f "$TESTER_HOME/socket.ini"
python3 -c 'import socket, sys; socket.socket(socket.AF_UNIX).bind(sys.argv[1])' "$TESTER_HOME/socket.ini"
chown tester:tester "$TESTER_HOME/socket.ini"
CFG=$TESTER_HOME/socket.ini WAIT_FOR='Could not open config file' run_keys f25-unopenable +close_message_box
ok=1
grep -q "Could not open config file $TESTER_HOME/socket.ini: No such device or address" "$out/f25-unopenable.log" \
    && ran_clean f25-unopenable 1 && ok=0
result "a config that cannot be opened stops the launcher with the reason (exit $(cat "$out/f25-unopenable.code"))" $ok
grep 'Could not open config file' "$out/f25-unopenable.log" | sed 's/^/      /'

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
# and Entry5 (:select) are refused, each with a log line saying why, which the leak pass needs:
# a refused entry frees its strings.
run_quick f25-empty
ok=1
ran_clean f25-empty && grep -q "Menu 'Main': 'Entry2' is empty, ignoring it" "$out/f25-empty.log" \
    && grep -q "Menu 'Main': 'Entry4' needs a title, an icon and a command, ignoring it" "$out/f25-empty.log" \
    && grep -q "Menu 'Main': 'Entry5' uses :select, which only a hotkey or gamepad button can, ignoring it" "$out/f25-empty.log" \
    && grep -A1 'Menu Name: Main' "$out/f25-empty.log" | grep -qE 'Number of Entries: 2$' && ok=0
result "an empty entry is skipped instead of crashing, and refused ones are dropped (exit $(cat "$out/f25-empty.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f25-empty.err" | sed 's/^/      /'
