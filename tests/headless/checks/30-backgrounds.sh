# The background's startup paths, which now all go through reload_background(): an image, a
# missing image, a slideshow, a slideshow folder with one image, one with none, and none at all.
# The setting is kept as chosen even when the launcher falls back to the colour.

run_quick f30-image
ok=1
[ "$(cat "$out/f30-image.code")" = 0 ] && ! grep -q "Couldn't load background image" "$out/f30-image.log" \
    && sanitizer_clean f30-image && ok=0
result "an image background loads" $ok

run_quick f30-missing
ok=1
[ "$(cat "$out/f30-missing.code")" = 0 ] && grep -q "Couldn't load background image" "$out/f30-missing.log" \
    && grep -A2 'Background ===' "$out/f30-missing.log" | grep -qE 'Mode:\s+Image$' \
    && sanitizer_clean f30-missing && ok=0
result "a missing image falls back to the colour, and the setting stays Image" $ok

run_quick f30-slideshow
ok=1
[ "$(cat "$out/f30-slideshow.code")" = 0 ] && grep -q "Found 3 images in directory /home/tester/Pictures" "$out/f30-slideshow.log" \
    && sanitizer_clean f30-slideshow && ok=0
result "a slideshow finds its three images" $ok

run_quick f30-one
ok=1
[ "$(cat "$out/f30-one.code")" = 0 ] && grep -q "Only one image found" "$out/f30-one.log" \
    && grep -A4 'Background ===' "$out/f30-one.log" | grep -qE 'Image:\s+\(null\)$' \
    && sanitizer_clean f30-one && ok=0
result "a one-image slideshow shows the image without rewriting the Image setting" $ok

run_quick f30-empty
ok=1
[ "$(cat "$out/f30-empty.code")" = 0 ] && grep -q "No images found in slideshow directory" "$out/f30-empty.log" \
    && sanitizer_clean f30-empty && ok=0
result "an empty slideshow folder falls back to the colour" $ok

run_quick f30-nodir
ok=1
[ "$(cat "$out/f30-nodir.code")" = 0 ] && grep -q "does not exist" "$out/f30-nodir.log" \
    && sanitizer_clean f30-nodir && ok=0
result "Mode=Slideshow with no SlideshowDirectory falls back instead of crashing (exit $(cat "$out/f30-nodir.code"))" $ok

# The slideshow loader. It runs on its own thread, so when its folder stops giving it two pictures
# it only reports, and the main thread falls back: to the one picture that still loads, or to the
# colour. The Mode setting stays Slideshow.

# A function to run a slideshow fixture that keeps running: wait until its first picture is up,
# run the rest of the arguments as a command (which may take the pictures away), then give the
# loader time to try the next picture (the fixtures change every 5 s, the shortest allowed)
# before quitting it. A launcher that hangs is killed.
run_slideshow() {
    local name=$1; shift
    local pid i
    rm -f "$LOG"
    "${TESTER[@]}" "$exe" -c "$FX/$name.ini" -d > "$out/$name.out" 2> "$out/$name.err" &
    pid=$!
    for i in $(seq 100); do grep -q 'Background set up: Slideshow' "$LOG" 2> /dev/null && break; sleep 0.2; done
    "$@"
    sleep 8
    kill -TERM "$pid" 2> /dev/null
    for i in $(seq 50); do kill -0 "$pid" 2> /dev/null || break; sleep 0.2; done
    kill -KILL "$pid" 2> /dev/null
    wait "$pid"; echo $? > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

# At startup: no file in the folder loads
run_quick f30-broken
ok=1
[ "$(cat "$out/f30-broken.code")" = 0 ] \
    && grep -q "Could not load any image from slideshow directory /home/tester/broken" "$out/f30-broken.log" \
    && grep -q "Background set up: Color" "$out/f30-broken.log" \
    && sanitizer_clean f30-broken && ok=0
result "a slideshow whose files all fail to load falls back to the colour instead of hanging (exit $(cat "$out/f30-broken.code"))" $ok

# While running: the only picture that loads is the one on show
run_slideshow f30-mixed true
ok=1
[ "$(cat "$out/f30-mixed.code")" = 0 ] \
    && grep -q "Could only load one image from slideshow directory /home/tester/mixed, showing it as a single image" "$out/f30-mixed.log" \
    && sanitizer_clean f30-mixed && ok=0
result "a running slideshow left with one picture shows it as a single image (exit $(cat "$out/f30-mixed.code"))" $ok

# While running: the pictures vanish (a network share dropping, say)
mkdir -p "$TESTER_HOME/vanish"
cp "$TESTER_HOME/Pictures/blue.png" "$TESTER_HOME/Pictures/green.png" "$TESTER_HOME/vanish/"
chown -R tester:tester "$TESTER_HOME/vanish"
run_slideshow f30-vanish rm -f "$TESTER_HOME/vanish/blue.png" "$TESTER_HOME/vanish/green.png"
ok=1
[ "$(cat "$out/f30-vanish.code")" = 0 ] \
    && grep -q "Could not load any image from slideshow directory /home/tester/vanish" "$out/f30-vanish.log" \
    && sanitizer_clean f30-vanish && ok=0
result "a running slideshow whose pictures vanish falls back to the colour (exit $(cat "$out/f30-vanish.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f30-vanish.err" | sed 's/^/      /'
