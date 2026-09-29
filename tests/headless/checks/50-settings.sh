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

# A read-only config so deep that its failure message wraps past the column (W is the widest
# letter): the message is cut in the middle to fit, so all three rows stay on show, and Leave
# without saving still closes. Its log lines, longer than the log's line buffer, are cut there.
long=$TESTER_HOME/cfg/long
for i in 1 2 3 4; do long=$long/$(printf 'W%.0s' $(seq 230)); done
mkdir -p "$long"
cp "$FX/f50-grid.ini" "$long/f50-grid.ini"
chown -R tester:tester "$TESTER_HOME/cfg"
chmod 444 "$long/f50-grid.ini"
CFG=$long/f50-grid.ini run_keys f50-longpath $ALL_MENUS_COLUMNS_UP Down Return
ok=1
cmp -s "$FX/f50-grid.ini" "$long/f50-grid.ini" && grep -q "Couldn't save to $TESTER_HOME/cfg/long/WWW" "$out/f50-longpath.log" \
    && in_range "$out/f50-longpath.log" "Couldn't save to" 'Settings: leaving without saving' 'Settings: rows 0 to 2 of 3 on show' \
    && grep -q 'Settings: leaving without saving' "$out/f50-longpath.log" && ran_clean f50-longpath && ok=0
result "settings: a failed save's long message is cut to fit, and its rows stay on show (exit $(cat "$out/f50-longpath.code"))" $ok
grep -E 'Settings: (rows|the note)' "$out/f50-longpath.log" | sed 's/^/      /'

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

# A held Menu key opens settings once, and they stay open: the keyboard's repeats of it do nothing.
# The same for a remote's Menu button, SDLK_MENU (X's XF86MenuKB, where Menu is SDLK_APPLICATION).
# The key must have come more than once, or nothing was repeated and the check proves nothing.
hold_menu() { xdotool keydown Menu; sleep 2; xdotool keyup Menu; sleep 1; }
hold_menukb() { xdotool keydown XF86MenuKB; sleep 2; xdotool keyup XF86MenuKB; sleep 1; }
for held in held:Menu:40000065 heldkb:MenuKB:40000076; do
    IFS=: read -r name key code <<< "$held"
    CFG=$FX/f50-keys.ini run_keys f50-$name +hold_$(tr 'A-Z' 'a-z' <<< "$key")
    n=$(grep -c "Key Menu (#$code) detected" "$out/f50-$name.log")
    ok=1
    [ "$n" -gt 1 ] && [ "$(grep -c 'Settings opened' "$out/f50-$name.log")" = 1 ] \
        && ! grep -q 'Settings closed' "$out/f50-$name.log" && ran_clean f50-$name && ok=0
    result "settings: a held $key key (#$code) opens them once, and they stay open (exit $(cat "$out/f50-$name.code"))" $ok
    echo "      the key came $n times; settings opened $(grep -c 'Settings opened' "$out/f50-$name.log") times"
done

# A remote's Menu button opens settings as the keyboard's Menu key does, and closes them again
CFG=$FX/f50-keys.ini run_keys f50-menukb XF86MenuKB XF86MenuKB
ok=1
grep -q 'Key Menu (#40000076) detected' "$out/f50-menukb.log" \
    && in_range "$out/f50-menukb.log" 'Key Menu (#40000076) detected' 'Settings closed' "Settings opened over menu 'Main'" \
    && ran_clean f50-menukb && ok=0
result "settings: the Menu key's other code (#40000076) opens and closes them (exit $(cat "$out/f50-menukb.code"))" $ok

# A gamepad's Start button held for 2 s opens settings once. The harness has no gamepad, so its
# build's test hook attaches a virtual one (STREAMFLEX_TEST_PAD), whose Start is held while the
# file that names exists.
hold_start() { : > /tmp/pad-start; sleep 2; rm -f /tmp/pad-start; sleep 1; }
rm -f /tmp/pad-start
STREAMFLEX_TEST_PAD=/tmp/pad-start WAIT_FOR='Gamepad connected' run_keys f50-padheld +hold_start
ok=1
[ "$(grep -c 'Gamepad ButtonStart detected' "$out/f50-padheld.log")" = 1 ] \
    && [ "$(grep -c 'Settings opened' "$out/f50-padheld.log")" = 1 ] \
    && ! grep -q 'Settings closed' "$out/f50-padheld.log" && ran_clean f50-padheld && ok=0
result "settings: a held Start button opens them once, and they stay open (exit $(cat "$out/f50-padheld.code"))" $ok
echo "      settings opened $(grep -c 'Settings opened' "$out/f50-padheld.log") times"

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

# An install whose bundled font is missing still opens settings, in the title font its config
# names (the launcher itself starts on that font), and the log says so
rm -rf /opt/sf-nofont
mkdir -p /opt/sf-nofont
cp /work/build/streamflex /opt/sf-nofont/ && cp -r /work/build/assets /opt/sf-nofont/
rm -f /opt/sf-nofont/assets/fonts/OpenSans-Regular.ttf
exe=/opt/sf-nofont/streamflex CFG=$FX/f50-nofont.ini run_keys f50-nofont Menu
ok=1
grep -q 'Settings: the font OpenSans-Regular.ttf is missing, so they use /opt/sf-nofont/assets/fonts/DejaVuSans.ttf' "$out/f50-nofont.log" \
    && grep -q "Settings opened over menu 'Main'" "$out/f50-nofont.log" && ran_clean f50-nofont && ok=0
result "settings: an install without the bundled font opens them in the title font (exit $(cat "$out/f50-nofont.code"))" $ok
grep 'Settings' "$out/f50-nofont.log" | sed 's/^/      /'

# Moving quickly down the Menus list (five menus of twelve 556 px SVG icons) does not lay out, or
# rasterize the icons of, each menu on the way: the preview follows once the cursor rests, on D.
# Going up to C and back to D shows D again without rendering its buttons a second time.
fast_downs() { xdotool key --delay 100 Down Down Down Down Down; sleep 2; }
revisit() { xdotool key Up; sleep 1; xdotool key Down; sleep 1; }
CFG=$FX/f50-menus.ini run_keys f50-menus Menu Down Return +fast_downs +revisit Menu
log=$out/f50-menus.log
ok=1
in_range "$log" 'Settings opened' 'Settings closed' "Settings: the preview shows menu 'D'" \
    && ! sed -n "/Settings opened/,/Settings: the preview shows menu 'D'/p" "$log" | grep -qE "Loading menu '[ABC]'" \
    && [ "$(grep -c "Menu 'D': rendered its buttons" "$log")" = 1 ] && ran_clean f50-menus && ok=0
result "settings: the Menus list's preview waits for the cursor to rest, and never renders a menu twice (exit $(cat "$out/f50-menus.code"))" $ok
grep -E "Loading menu|rendered its buttons|the preview shows menu|kept the screen waiting" "$log" | sed 's/^/      /'
