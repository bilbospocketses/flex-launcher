# The settings screen, driven by key presses: Menu opens it, the arrows, Return and BackSpace
# move through it, and Back at the top saves. Each check that saves starts from a fresh copy of
# its fixture that the test user can write (writable_config, in run.sh).

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
    && grep -q 'Key .* (#40000065) detected' "$out/f50-grid.log" && ran_clean f50-grid && ok=0
result "settings: a grid change saves one line, keeps its comment, and keeps a backup (exit $(cat "$out/f50-grid.code"))" $ok
diff "$FX/f50-grid.ini" "$cfg" | sed 's/^/      /'

# [Layout] with both Columns and its older name MaxButtons: a Columns change leaves only Columns
cfg=$(writable_config f50-alias)
CFG=$cfg run_keys f50-alias $ALL_MENUS_COLUMNS_UP
ok=1
[ "$(sed -n '/^\[Layout\]/,/^\[/p' "$cfg" | grep -c '^Columns=')" = 1 ] && grep -qx 'Columns=5' "$cfg" \
    && ! grep -q '^MaxButtons=' "$cfg" && grep -qx 'Rows=1' "$cfg" \
    && grep -q 'Settings: \[Layout\] Columns 4 -> 5' "$out/f50-alias.log" && ran_clean f50-alias && ok=0
result "settings: a Columns change removes an older MaxButtons beside it (exit $(cat "$out/f50-alias.code"))" $ok
diff "$FX/f50-alias.ini" "$cfg" | sed 's/^/      /'

# Stepping a menu's Rows down to "All menus" removes its line
cfg=$(writable_config f50-grid)
CFG=$cfg run_keys f50-inherit Menu Down Return Down Down Return Left Left Left BackSpace BackSpace BackSpace
ok=1
[ "$(changed_lines "$FX/f50-grid.ini" "$cfg")" = 1 ] && ! sed -n '/^\[Games\]/,$p' "$cfg" | grep -q '^Rows=' \
    && grep -q 'Settings: \[Games\] Rows 1 -> (none)' "$out/f50-inherit.log" && ran_clean f50-inherit && ok=0
result "settings: a menu's Rows set to All menus removes the line (exit $(cat "$out/f50-inherit.code"))" $ok

# Discard puts everything back, and closing with nothing changed writes nothing
cfg=$(writable_config f50-grid)
CFG=$cfg run_keys f50-discard Menu Down Return Return Down Right BackSpace BackSpace Down Down Return BackSpace
ok=1
cmp -s "$FX/f50-grid.ini" "$cfg" && [ ! -e "$cfg.bak" ] \
    && in_range "$out/f50-discard.log" 'Settings: discarded the changes' 'Settings closed' "Menu 'Main': 4 x 1 grid" \
    && grep -q 'Settings: nothing changed' "$out/f50-discard.log" && ran_clean f50-discard && ok=0
result "settings: Discard, then Back, leaves the file untouched (exit $(cat "$out/f50-discard.code"))" $ok

# A config the launcher cannot write: the failure rows, then Leave without saving
cfg=$(writable_config f50-grid)
chmod 444 "$cfg"
CFG=$cfg run_keys f50-readonly $ALL_MENUS_COLUMNS_UP Down Return
ok=1
cmp -s "$FX/f50-grid.ini" "$cfg" && [ ! -e "$cfg.bak" ] && [ ! -e "$cfg.tmp" ] && [ ! -e "$cfg.bak.tmp" ] \
    && grep -q "Couldn't save to $cfg: permission denied" "$out/f50-readonly.log" \
    && grep -q 'Settings: leaving without saving' "$out/f50-readonly.log" && ran_clean f50-readonly && ok=0
result "settings: a read-only config shows why, and Leave without saving closes (exit $(cat "$out/f50-readonly.code"))" $ok

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
    && ran_clean f50-system && ok=0
result "settings: a read-only system config is saved as ~/.config/streamflex/config.ini (exit $(cat "$out/f50-system.code"))" $ok
rm -rf "$TESTER_HOME/.config/streamflex"

# While settings are open, the Esc=:quit hotkey is ignored, and the Menu key closes them again
run_keys f50-hotkey Menu Escape Menu
ok=1
in_range "$out/f50-hotkey.log" 'Settings opened' 'Settings closed' "Settings: ignoring ':quit' while settings are open" \
    && ran_clean f50-hotkey && ok=0
result "settings: a :quit hotkey is ignored while they are open, and Menu closes them (exit $(cat "$out/f50-hotkey.code"))" $ok

# Quitting while settings are open closes them first, so the QuitCmd still runs
run_keys f50-quitcmd Menu
ok=1
grep -q 'Settings opened' "$out/f50-quitcmd.log" \
    && sed -n '/Quitting program/,$p' "$out/f50-quitcmd.log" | grep -q "Loading menu 'Main'" \
    && ! grep -q 'Settings: ignoring' "$out/f50-quitcmd.log" && ran_clean f50-quitcmd && ok=0
result "settings: quitting while they are open still runs the QuitCmd (exit $(cat "$out/f50-quitcmd.code"))" $ok

# A screensaver already on when settings open goes off with the key that opened them, not later.
# Startup time varies, so the keys wait for the screensaver to come on rather than for a clock.
run_after_line f50-screensaver 'Screensaver on' Menu BackSpace
ok=1
precedes "$out/f50-screensaver.log" 'Screensaver on' 'Settings opened' \
    && in_range "$out/f50-screensaver.log" 'Settings opened' 'Settings closed' 'Screensaver off' \
    && ran_clean f50-screensaver && ok=0
result "settings: a screensaver on when they open goes off as they open (exit $(cat "$out/f50-screensaver.code"))" $ok
grep -E 'Screensaver o(n|ff)|Settings (opened|closed)' "$out/f50-screensaver.log" | sed 's/^/      /'

# A menu with no entries cannot be previewed: the preview stays put, and its grid still saves
cfg=$(writable_config f50-empty)
CFG=$cfg run_keys f50-empty Menu Down Return Down Down Return Right BackSpace BackSpace BackSpace
ok=1
ran_clean f50-empty && sed -n '/^\[Empty\]/,$p' "$cfg" | grep -qx 'Rows=3' \
    && grep -q 'Settings: \[Empty\] Rows 2 -> 3' "$out/f50-empty.log" \
    && ! grep -q "Loading menu 'Empty'" "$out/f50-empty.log" && ok=0
result "settings: a menu with no entries can have its grid changed (exit $(cat "$out/f50-empty.code"))" $ok

# The gamepad's Start button opens settings when nothing else does, and keeps a mapping of its own
run_quick f50-pad
run_quick f50-pad-taken
ok=1
grep -A12 'Gamepad ===' "$out/f50-pad.log" | grep -qE 'ButtonStart\s+:settings$' \
    && grep -A12 'Gamepad ===' "$out/f50-pad-taken.log" | grep -qE 'ButtonStart\s+:quit$' \
    && ! grep -A12 'Gamepad ===' "$out/f50-pad-taken.log" | grep -q ':settings' \
    && ran_clean f50-pad && ran_clean f50-pad-taken && ok=0
result "settings: Start opens them by default, unless the config maps Start itself (exit $(cat "$out/f50-pad.code") and $(cat "$out/f50-pad-taken.code"))" $ok
