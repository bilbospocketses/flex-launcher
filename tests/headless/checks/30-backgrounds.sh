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
