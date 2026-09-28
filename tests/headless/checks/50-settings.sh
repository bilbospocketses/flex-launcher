# The settings screen, driven by key presses: Menu opens it, the arrows, Return and BackSpace
# move through it, and Back at the top saves. Each check starts from a fresh copy of its fixture
# that the test user can write.

# A function to give the test user a copy of a fixture it can write; prints its path
writable_config() {
    mkdir -p "$TESTER_HOME/cfg"
    rm -f "$TESTER_HOME/cfg/$1.ini" "$TESTER_HOME/cfg/$1.ini.bak" "$TESTER_HOME/cfg/$1.ini.tmp" "$TESTER_HOME/cfg/$1.ini.bak.tmp"
    cp "$FX/$1.ini" "$TESTER_HOME/cfg/$1.ini"
    chown -R tester:tester "$TESTER_HOME/cfg"
    chmod 644 "$TESTER_HOME/cfg/$1.ini"
    echo "$TESTER_HOME/cfg/$1.ini"
}

# A function to count the lines that differ between two files (a changed line counts twice)
changed_lines() { diff "$1" "$2" | grep -c '^[<>]'; }

# The keys that open settings, go to All menus, step Columns up once and back out, saving
ALL_MENUS_COLUMNS_UP="Menu Down Return Return Down Right BackSpace BackSpace BackSpace"

# A grid change saves exactly one line, keeps its trailing comment, and leaves the old file as .bak
cfg=$(writable_config f50-grid)
CFG=$cfg run_keys f50-grid $ALL_MENUS_COLUMNS_UP
ok=1
[ "$(changed_lines "$FX/f50-grid.ini" "$cfg")" = 2 ] && grep -qx 'Columns=5 ; four across' "$cfg" \
    && cmp -s "$FX/f50-grid.ini" "$cfg.bak" && [ ! -e "$cfg.tmp" ] \
    && grep -q "Settings opened over menu 'Main'" "$out/f50-grid.log" \
    && grep -q 'Settings: \[Layout\] Columns 4 -> 5' "$out/f50-grid.log" \
    && grep -q "Settings saved 1 change(s) to $cfg (backup: $cfg.bak)" "$out/f50-grid.log" \
    && grep -q 'Key .* (#40000065) detected' "$out/f50-grid.log" && sanitizer_clean f50-grid && ok=0
result "settings: a grid change saves one line, keeps its comment, and keeps a backup" $ok
diff "$FX/f50-grid.ini" "$cfg" | sed 's/^/      /'

# [Layout] with both Columns and its older name MaxButtons: a Columns change leaves only Columns
cfg=$(writable_config f50-alias)
CFG=$cfg run_keys f50-alias $ALL_MENUS_COLUMNS_UP
ok=1
[ "$(sed -n '/^\[Layout\]/,/^\[/p' "$cfg" | grep -c '^Columns=')" = 1 ] && grep -qx 'Columns=5' "$cfg" \
    && ! grep -q '^MaxButtons=' "$cfg" && grep -qx 'Rows=1' "$cfg" \
    && grep -q 'Settings: \[Layout\] Columns 4 -> 5' "$out/f50-alias.log" && sanitizer_clean f50-alias && ok=0
result "settings: a Columns change removes an older MaxButtons beside it" $ok
diff "$FX/f50-alias.ini" "$cfg" | sed 's/^/      /'

# Stepping a menu's Rows down to "All menus" removes its line
cfg=$(writable_config f50-grid)
CFG=$cfg run_keys f50-inherit Menu Down Return Down Down Return Left Left Left BackSpace BackSpace BackSpace
ok=1
[ "$(changed_lines "$FX/f50-grid.ini" "$cfg")" = 1 ] && ! sed -n '/^\[Games\]/,$p' "$cfg" | grep -q '^Rows=' \
    && grep -q 'Settings: \[Games\] Rows 1 -> (none)' "$out/f50-inherit.log" && sanitizer_clean f50-inherit && ok=0
result "settings: a menu's Rows set to All menus removes the line" $ok

# Discard puts everything back, and closing with nothing changed writes nothing
cfg=$(writable_config f50-grid)
CFG=$cfg run_keys f50-discard Menu Down Return Return Down Right BackSpace BackSpace Down Down Return BackSpace
ok=1
cmp -s "$FX/f50-grid.ini" "$cfg" && [ ! -e "$cfg.bak" ] \
    && grep -q 'Settings: discarded the changes' "$out/f50-discard.log" \
    && sed -n '/Settings: discarded the changes/,/Settings closed/p' "$out/f50-discard.log" \
        | grep -q "Menu 'Main': 4 x 1 grid" \
    && grep -q 'Settings: nothing changed' "$out/f50-discard.log" && sanitizer_clean f50-discard && ok=0
result "settings: Discard, then Back, leaves the file untouched" $ok

# A config the launcher cannot write: the failure rows, then Leave without saving
cfg=$(writable_config f50-grid)
chmod 444 "$cfg"
CFG=$cfg run_keys f50-readonly $ALL_MENUS_COLUMNS_UP Down Return
ok=1
cmp -s "$FX/f50-grid.ini" "$cfg" && [ ! -e "$cfg.bak" ] && [ ! -e "$cfg.tmp" ] && [ ! -e "$cfg.bak.tmp" ] \
    && grep -q "Couldn't save to $cfg: permission denied" "$out/f50-readonly.log" \
    && grep -q 'Settings: leaving without saving' "$out/f50-readonly.log" && sanitizer_clean f50-readonly && ok=0
result "settings: a read-only config shows why, and Leave without saving closes" $ok

# The packaged system config is read-only: the first save becomes the user's own copy
rm -rf /opt/sf "$TESTER_HOME/.config/streamflex" /usr/local/share/streamflex
mkdir -p /opt/sf /usr/local/share/streamflex
cp /work/build/streamflex /opt/sf/ && cp -r /work/build/assets /opt/sf/
cp "$FX/f50-grid.ini" /usr/local/share/streamflex/config.ini
exe=/opt/sf/streamflex CFG=none run_keys f50-system $ALL_MENUS_COLUMNS_UP
user_cfg=$TESTER_HOME/.config/streamflex/config.ini
ok=1
cmp -s "$FX/f50-grid.ini" /usr/local/share/streamflex/config.ini && grep -qx 'Columns=5 ; four across' "$user_cfg" \
    && grep -q "Settings saved 1 change(s) to $user_cfg (backup: none)" "$out/f50-system.log" \
    && sanitizer_clean f50-system && ok=0
result "settings: a read-only system config is saved as ~/.config/streamflex/config.ini" $ok
rm -rf "$TESTER_HOME/.config/streamflex"

# While settings are open, the Esc=:quit hotkey is ignored, and the Menu key closes them again
run_keys f50-hotkey Menu Escape Menu
ok=1
grep -q "Settings: ignoring ':quit' while settings are open" "$out/f50-hotkey.log" \
    && grep -q 'Settings closed' "$out/f50-hotkey.log" && sanitizer_clean f50-hotkey && ok=0
result "settings: a :quit hotkey is ignored while they are open, and Menu closes them" $ok

# Quitting while settings are open closes them first, so the QuitCmd still runs
run_keys f50-quitcmd Menu
ok=1
grep -q 'Settings opened' "$out/f50-quitcmd.log" \
    && sed -n '/Quitting program/,$p' "$out/f50-quitcmd.log" | grep -q "Loading menu 'Main'" \
    && ! grep -q 'Settings: ignoring' "$out/f50-quitcmd.log" && sanitizer_clean f50-quitcmd && ok=0
result "settings: quitting while they are open still runs the QuitCmd" $ok

# A screensaver already on when settings open goes off with the key that opened them, not later.
# Startup time varies, so the keys wait for the screensaver to come on rather than for a clock.

# A function to run a config until its log shows a line (up to 20 s), then send the keys a second
# apart and quit it as run_keys does
run_after_line() {
    local name=$1 line=$2; shift 2
    local args; mapfile -t args < <(config_args "$name")
    rm -f "$LOG"
    "${TESTER[@]}" "$exe" "${args[@]}" -d > "$out/$name.out" 2> "$out/$name.err" &
    local pid=$! i
    for i in $(seq 100); do grep -q "$line" "$LOG" 2> /dev/null && break; sleep 0.2; done
    for k in "$@"; do xdotool key "$k"; sleep 1; done
    kill -TERM "$pid" 2> /dev/null
    for i in $(seq 50); do kill -0 "$pid" 2> /dev/null || break; sleep 0.2; done
    kill -KILL "$pid" 2> /dev/null
    wait "$pid"; echo $? > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

run_after_line f50-screensaver 'Screensaver on' Menu BackSpace
ok=1
sed -n '1,/Settings opened/p' "$out/f50-screensaver.log" | grep -q 'Screensaver on' \
    && sed -n '/Settings opened/,/Settings closed/p' "$out/f50-screensaver.log" | grep -q 'Screensaver off' \
    && sanitizer_clean f50-screensaver && ok=0
result "settings: a screensaver on when they open goes off as they open" $ok
grep -E 'Screensaver o(n|ff)|Settings (opened|closed)' "$out/f50-screensaver.log" | sed 's/^/      /'

# A menu with no entries cannot be previewed: the preview stays put, and its grid still saves
cfg=$(writable_config f50-empty)
CFG=$cfg run_keys f50-empty Menu Down Return Down Down Return Right BackSpace BackSpace BackSpace
ok=1
[ "$(cat "$out/f50-empty.code")" = 0 ] && sed -n '/^\[Empty\]/,$p' "$cfg" | grep -qx 'Rows=3' \
    && grep -q 'Settings: \[Empty\] Rows 2 -> 3' "$out/f50-empty.log" \
    && ! grep -q "Loading menu 'Empty'" "$out/f50-empty.log" && sanitizer_clean f50-empty && ok=0
result "settings: a menu with no entries can have its grid changed" $ok

# The gamepad's Start button opens settings when nothing else does, and keeps a mapping of its own
run_quick f50-pad
run_quick f50-pad-taken
ok=1
grep -A12 'Gamepad ===' "$out/f50-pad.log" | grep -qE 'ButtonStart\s+:settings$' \
    && grep -A12 'Gamepad ===' "$out/f50-pad-taken.log" | grep -qE 'ButtonStart\s+:quit$' \
    && ! grep -A12 'Gamepad ===' "$out/f50-pad-taken.log" | grep -q ':settings' && ok=0
result "settings: Start opens them by default, unless the config maps Start itself" $ok
