# The Background page: a preset colour, an image and a slideshow folder chosen in the folder
# browser, the incomplete-mode rule, and switching modes while a slideshow is running. Pixel
# checks read the preview off the screen (look, in run.sh) once the log says what it should show.

# The preview's background is read near its scene's top-left corner, clear of the menu. The
# checkerboard's squares are 60 px of the scene (1080 / 18): (30,30) and (90,90) are dark, the
# two squares beside them light. Black is also what an empty screen shows, so that probe reads the
# settings' own backdrop (#0B1620) in the screen's bottom-left corner too.
shows_black() { look "$1" "$2" black 'Settings opened' 30,30=0,0,0 @5,1074=11,22,32; }
shows_charcoal() { look "$1" "$2" charcoal 'Settings: [Background] Color #000000 -> #1E1E1E' 30,30=30,30,30; }
shows_blue() { look "$1" "$2" blue 'Settings: the preview shows /home/tester/Pictures/blue.png' 30,30=40,70,160; }
shows_green() { look "$1" "$2" green 'Settings: the preview shows /home/tester/Pictures/green.png' 30,30=40,140,70; }
shows_red() { look "$1" "$2" red 'Settings: the preview shows /home/tester/Pictures/red.png' 30,30=170,40,40; }
shows_checkerboard() {
    look "$1" "$2" checkerboard 'Settings: [Background] Mode Slideshow -> Transparent' \
        30,30=85,85,85 90,30=136,136,136 30,90=136,136,136 90,90=85,85,85
}

# Colour: step from Black to Charcoal, and the preview shows each
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f60-colour Menu +shows_black Return Down Right +shows_charcoal BackSpace BackSpace
ok=1
[ "$(changed_lines "$FX/f60-colour.ini" "$cfg")" = 2 ] && grep -qx 'Color=#1E1E1E' "$cfg" \
    && grep -q 'Settings: \[Background\] Color #000000 -> #1E1E1E' "$out/f60-colour.log" \
    && grep -qx 'black yes' "$out/f60-colour.seen" && grep -qx 'charcoal yes' "$out/f60-colour.seen" \
    && ran_clean f60-colour && ok=0
result "settings: a preset colour is saved, and the preview shows it (exit $(cat "$out/f60-colour.code"))" $ok
sed 's/^/      /' "$out/f60-colour.seen"

# The browser's highlighted image fills the preview once its decode is done: blue, green, red
CFG=$FX/f60-colour.ini run_keys f60-preview Menu Return Right Down Return +shows_blue Down +shows_green Down +shows_red Menu
ok=1
grep -qx 'blue yes' "$out/f60-preview.seen" && grep -qx 'green yes' "$out/f60-preview.seen" \
    && grep -qx 'red yes' "$out/f60-preview.seen" && grep -q 'Settings: nothing changed' "$out/f60-preview.log" \
    && ran_clean f60-preview && ok=0
result "settings: the preview shows the highlighted image (exit $(cat "$out/f60-preview.code"))" $ok
sed 's/^/      /' "$out/f60-preview.seen"

# Transparent shows the checkerboard; stepping back to Colour leaves nothing to save
CFG=$FX/f60-colour.ini run_keys f60-transparent Menu Return Right Right Right +shows_checkerboard Left Left Left BackSpace BackSpace
ok=1
grep -qx 'checkerboard yes' "$out/f60-transparent.seen" && grep -q 'Settings: nothing changed' "$out/f60-transparent.log" \
    && ran_clean f60-transparent && ok=0
result "settings: the Transparent preview is a checkerboard (exit $(cat "$out/f60-transparent.code"))" $ok
sed 's/^/      /' "$out/f60-transparent.seen"

# Image: Mode to Image, open the browser (it starts in Pictures), take the second image
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f60-image Menu Return Right Down Return Down Return BackSpace BackSpace
ok=1
grep -qx 'Mode=Image' "$cfg" && grep -qx 'Image=/home/tester/Pictures/green.png' "$cfg" \
    && grep -q 'Settings saved 2 change(s)' "$out/f60-image.log" && ran_clean f60-image && ok=0
result "settings: an image chosen in the folder browser is saved (exit $(cat "$out/f60-image.code"))" $ok
diff "$FX/f60-colour.ini" "$cfg" | sed 's/^/      /'

# OK while the highlighted image is still decoding: the test hook (STREAMFLEX_TEST_DECODE_DELAY_MS,
# in the harness's build only) makes each decode take 4 s, so OK lands while blue.png, the image
# highlighted first, is still decoding and green.png waits behind it. OK waits for both, and
# chooses green, the one highlighted, only once green's decode is done.
cfg=$(writable_config f60-colour)
STREAMFLEX_TEST_DECODE_DELAY_MS=4000 CFG=$cfg UNTIL='Settings saved' \
    run_keys f60-slowdecode Menu Return Right Down Return Down Return BackSpace BackSpace
ok=1
grep -qx 'Image=/home/tester/Pictures/green.png' "$cfg" \
    && grep -q 'Settings: OK waited for the decode of /home/tester/Pictures/green.png' "$out/f60-slowdecode.log" \
    && precedes "$out/f60-slowdecode.log" 'Settings: the preview shows /home/tester/Pictures/green.png' \
        'Settings: chose /home/tester/Pictures/green.png' \
    && ran_clean f60-slowdecode && ok=0
result "settings: OK during a slow decode waits for it and chooses the highlighted image (exit $(cat "$out/f60-slowdecode.code"))" $ok
grep -E 'Settings: (OK waited|chose|the preview shows)' "$out/f60-slowdecode.log" | sed 's/^/      /'

# The same with a broken image: OK on b.png while it is still decoding waits, finds it cannot be
# opened, and refuses it. Without the wait it would be chosen and saved.
STREAMFLEX_TEST_DECODE_DELAY_MS=4000 CFG=$FX/f60-broken.ini UNTIL='Settings: nothing changed' \
    run_keys f60-slowbroken Menu Return Down Return Down Return Menu
ok=1
grep -q 'Settings: OK waited for the decode of /home/tester/broken/b.png' "$out/f60-slowbroken.log" \
    && grep -q 'Settings: This image cannot be opened: /home/tester/broken/b.png' "$out/f60-slowbroken.log" \
    && ! grep -q 'Settings: chose' "$out/f60-slowbroken.log" && ran_clean f60-slowbroken && ok=0
result "settings: OK on a broken image during its decode waits, then refuses it (exit $(cat "$out/f60-slowbroken.code"))" $ok

# Image with none chosen: leaving the page puts Colour back, so nothing is saved
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f60-incomplete Menu Return Right BackSpace BackSpace
ok=1
cmp -s "$FX/f60-colour.ini" "$cfg" && grep -q 'Settings: \[Background\] Mode Image -> Color' "$out/f60-incomplete.log" \
    && grep -q 'Settings: nothing changed' "$out/f60-incomplete.log" && ran_clean f60-incomplete && ok=0
result "settings: Image with no image chosen goes back to Colour and saves nothing (exit $(cat "$out/f60-incomplete.code"))" $ok

# Slideshow: Mode to Slideshow, open the browser on the Folder row, use Pictures
cfg=$(writable_config f60-colour)
CFG=$cfg run_keys f60-slideshow Menu Return Right Right Down Return Return BackSpace BackSpace
ok=1
grep -qx 'Mode=Slideshow' "$cfg" && grep -qx 'SlideshowDirectory=/home/tester/Pictures' "$cfg" \
    && grep -q 'Found 3 images in directory /home/tester/Pictures' "$out/f60-slideshow.log" \
    && ran_clean f60-slideshow && ok=0
result "settings: a slideshow folder chosen in the folder browser is saved (exit $(cat "$out/f60-slideshow.code"))" $ok

# Stepping the mode through a running slideshow: its first change is due at 5 s and fades for 3 s.
# The keys start when the log says the fade has begun, so settings open inside it and the first
# step (to Transparent) lands in it 2 s later, however slowly the launcher started: the log must
# say the fade in progress was dropped.
run_after_line f60-running 'Slideshow: fading in the next image' Menu Return Right Left Right Left BackSpace BackSpace
ok=1
ran_clean f60-running && grep -q 'Settings: nothing changed' "$out/f60-running.log" \
    && precedes "$out/f60-running.log" 'Slideshow: fading in the next image' 'Settings: [Background] Mode Slideshow -> Transparent' \
    && in_range "$out/f60-running.log" 'Settings: [Background] Mode Slideshow -> Transparent' \
        'Background set up' 'Slideshow: dropped the fade in progress' \
    && ok=0
result "settings: switching modes while a slideshow runs frees its fade cleanly (exit $(cat "$out/f60-running.code"))" $ok
grep -m3 -E 'AddressSanitizer|runtime error' "$out/f60-running.err" | sed 's/^/      /'

# A function to stand in for a slow disk under the named pipe PIPE. Opening a pipe to read it
# waits until something opens it to write, so a read of it waits until this function answers
# (and then fails at once: SDL_image cannot use a file it cannot seek in). It answers the
# slideshow's first image, read on the main thread as the launcher starts, at once. Then it
# answers nothing until the file RELEASE exists, which holds the slideshow's loader thread in its
# read of PIPE (and writes HELD once that thread is running), and after that everything at once.
hold_pipe() {
    local pipe=$1 held=$2 release=$3
    until grep -q 'Background set up' "$LOG" 2> /dev/null; do
        python3 -c 'import os, sys; os.close(os.open(sys.argv[1], os.O_WRONLY | os.O_NONBLOCK))' "$pipe" 2> /dev/null
        sleep 0.1
    done
    until [ -e "$release" ]; do
        grep -q '^Slideshow' /proc/[0-9]*/task/*/comm 2> /dev/null && : > "$held"
        sleep 0.2
    done
    while :; do exec 3> "$pipe"; exec 3>&-; done
}
# Functions for +keys: wait up to 30 s for hold_pipe to hold the loader thread, and release it
loader_held() { local i; for i in $(seq 150); do [ -e /tmp/loader-held ] && return; sleep 0.2; done; }
release_loader() { : > /tmp/loader-release; }

# Stepping the mode while the slideshow's loader thread is still reading the next image. In
# ~/loading, a.png is a picture and b.png a pipe that hold_pipe answers. Whichever the shuffle
# puts first, only a.png loads as the launcher starts, so the loader's first read is b.png, and
# it is held until after the step. The step must wait for the thread and drop the image it read
# (a.png again, once b.png fails), before any fade began.
rm -rf "$TESTER_HOME/loading" /tmp/loader-held /tmp/loader-release "$LOG"
mkdir -p "$TESTER_HOME/loading"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/loading/a.png"
mkfifo "$TESTER_HOME/loading/b.png"
chown -R tester:tester "$TESTER_HOME/loading"
hold_pipe "$TESTER_HOME/loading/b.png" /tmp/loader-held /tmp/loader-release &
holder=$!
UNTIL='Settings: nothing changed' run_keys f60-loading +loader_held Menu Return Right +release_loader Left BackSpace BackSpace
kill "$holder"; wait "$holder" 2> /dev/null
ok=1
[ -e /tmp/loader-held ] && ran_clean f60-loading \
    && ! precedes "$out/f60-loading.log" 'Slideshow: fading in the next image' 'Settings: [Background] Mode Slideshow -> Transparent' \
    && in_range "$out/f60-loading.log" 'Settings: [Background] Mode Slideshow -> Transparent' \
        'Background set up' 'Slideshow: dropped the fade in progress' \
    && ok=0
result "settings: switching modes while the slideshow loads its next image drops that image (exit $(cat "$out/f60-loading.code"))" $ok
[ -e /tmp/loader-held ] || echo "      the loader thread was never held on b.png"

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
    && grep -q 'Settings: nothing changed' "$out/f60-locked.log" && ran_clean f60-locked && ok=0
result "settings: a folder that cannot be opened says why (exit $(cat "$out/f60-locked.code"))" $ok
grep -E "Settings: (browsing|Can't)" "$out/f60-locked.log" | sed 's/^/      /'

# Images that only look like pictures (~/broken): the browser opens on a.png, whose failed decode
# puts "cannot be opened" in the caption with no key pressed; Down moves to b.png, whose OK refuses
run_keys f60-broken Menu Return Down Return Down Return Menu
ok=1
grep -q 'Settings: the caption says This image cannot be opened for /home/tester/broken/a.png' "$out/f60-broken.log" \
    && grep -q 'Settings: This image cannot be opened: /home/tester/broken/b.png' "$out/f60-broken.log" \
    && ! grep -q 'Settings: chose' "$out/f60-broken.log" && grep -q 'Settings: nothing changed' "$out/f60-broken.log" \
    && ran_clean f60-broken && ok=0
result "settings: a broken image says so as soon as it is highlighted, and cannot be chosen (exit $(cat "$out/f60-broken.code"))" $ok
grep -E 'Settings: (browsing|could not|the caption|This image|chose)' "$out/f60-broken.log" | sed 's/^/      /'
