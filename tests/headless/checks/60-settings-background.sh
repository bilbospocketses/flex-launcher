# The Background page: a preset colour, an image and a slideshow folder chosen in the folder
# browser, the incomplete-mode rule, and switching modes while a slideshow is running.
# writable_config, changed_lines and run_after_line come from 50-settings.sh, which runs first.

# Colour: step from Black to Charcoal
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f60-colour Menu Return Down Right BackSpace BackSpace
ok=1
[ "$(changed_lines "$FX/f60-colour.ini" "$cfg")" = 2 ] && grep -qx 'Color=#1E1E1E' "$cfg" \
    && grep -q 'Settings: \[Background\] Color #000000 -> #1E1E1E' "$out/f60-colour.log" && sanitizer_clean f60-colour && ok=0
result "settings: a preset colour is saved" $ok

# Image: Mode to Image, open the browser (it starts in Pictures), take the second image
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f60-image Menu Return Right Down Return Down Return BackSpace BackSpace
ok=1
grep -qx 'Mode=Image' "$cfg" && grep -qx 'Image=/home/tester/Pictures/green.png' "$cfg" \
    && grep -q 'Settings saved 2 change(s)' "$out/f60-image.log" && sanitizer_clean f60-image && ok=0
result "settings: an image chosen in the folder browser is saved" $ok
diff "$FX/f60-colour.ini" "$cfg" | sed 's/^/      /'

# Image with none chosen: leaving the page puts Colour back, so nothing is saved
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f60-incomplete Menu Return Right BackSpace BackSpace
ok=1
cmp -s "$FX/f60-colour.ini" "$cfg" && grep -q 'Settings: \[Background\] Mode Image -> Color' "$out/f60-incomplete.log" \
    && grep -q 'Settings: nothing changed' "$out/f60-incomplete.log" && sanitizer_clean f60-incomplete && ok=0
result "settings: Image with no image chosen goes back to Colour and saves nothing" $ok

# Slideshow: Mode to Slideshow, open the browser on the Folder row, use Pictures
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f60-slideshow Menu Return Right Right Down Return Return BackSpace BackSpace
ok=1
grep -qx 'Mode=Slideshow' "$cfg" && grep -qx 'SlideshowDirectory=/home/tester/Pictures' "$cfg" \
    && grep -q 'Found 3 images in directory /home/tester/Pictures' "$out/f60-slideshow.log" \
    && sanitizer_clean f60-slideshow && ok=0
result "settings: a slideshow folder chosen in the folder browser is saved" $ok

# Stepping the mode through a running slideshow: its first change is due at 5 s and fades for 3 s.
# The keys start when the log says the fade has begun, so settings open inside it and the first
# step (to Transparent) lands in it 2 s later, however slowly the launcher started.
run_after_line f60-running 'Slideshow: fading in the next image' Menu Return Right Left Right Left BackSpace BackSpace
ok=1
[ "$(cat "$out/f60-running.code")" = 0 ] && grep -q 'Settings: nothing changed' "$out/f60-running.log" \
    && sed -n '/Slideshow: fading in the next image/,$p' "$out/f60-running.log" \
        | grep -q 'Settings: \[Background\] Mode Slideshow -> Transparent' \
    && sanitizer_clean f60-running && ok=0
result "settings: switching modes while a slideshow runs frees it cleanly" $ok
grep -m3 -E 'AddressSanitizer|runtime error' "$out/f60-running.err" | sed 's/^/      /'

# The Folder row shows the folder's name and its image count: counted when the page opens (the
# running slideshow's Pictures) and again when a folder is chosen in the browser
DOT=$(printf '\xC2\xB7')
ok=1
grep -q "Settings: the Folder row shows Pictures $DOT 3 images" "$out/f60-running.log" \
    && sed -n '/Settings: chose \/home\/tester\/Pictures/,$p' "$out/f60-slideshow.log" \
        | grep -q "Settings: the Folder row shows Pictures $DOT 3 images" && ok=0
result "settings: the Folder row shows the folder's name and its image count" $ok

# A folder the test user cannot read: OK on it stays in the browser and says why
rm -rf "$TESTER_HOME/locking"
mkdir -p "$TESTER_HOME/locking/locked"
chown -R tester:tester "$TESTER_HOME/locking"
chmod 000 "$TESTER_HOME/locking/locked"
run_keys f60-locked Menu Return Down Return Down Return Menu
ok=1
grep -q "Settings: Can't open locked: permission denied" "$out/f60-locked.log" \
    && grep -q 'Settings: nothing changed' "$out/f60-locked.log" && sanitizer_clean f60-locked && ok=0
result "settings: a folder that cannot be opened says why" $ok
grep -E "Settings: (browsing|Can't)" "$out/f60-locked.log" | sed 's/^/      /'
