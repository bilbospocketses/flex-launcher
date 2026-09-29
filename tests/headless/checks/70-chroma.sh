# Transparent mode on Windows makes every pixel of the window that is exactly the chroma key colour
# see-through, so an icon's own pixels of that colour were holes in it (the Plex icon's diagonal).
# Every icon is now kept off the key as it loads. Xvfb has no see-through windows, but it draws the
# key colour as the background, so the proof is on screen: inside each icon (found by its frame's
# colour) no pixel may be the key colour. Three icons: an RGBA PNG, an RGB PNG (converted to RGBA
# first) and an SVG, each a square of #010101 in a frame whose red is 0, so no blend of the two
# as the icon is scaled can be the key either.
rm -rf "$TESTER_HOME/keyed"
mkdir -p "$TESTER_HOME/keyed"
python3 "$HERE/make_images.py" --keyed "$TESTER_HOME/keyed/rgba.png" 0,200,200 rgba
python3 "$HERE/make_images.py" --keyed "$TESTER_HOME/keyed/rgb.png" 0,200,100 rgb
printf '%s\n' '<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64">' \
    '<rect width="64" height="64" fill="#0064C8"/><rect x="16" y="16" width="32" height="32" fill="#010101"/></svg>' \
    > "$TESTER_HOME/keyed/svg.svg"
chown -R tester:tester "$TESTER_HOME/keyed"

# A function for a +key (see run_keys): give the window the screen, then count the key-coloured
# pixels inside each icon, once all three frames are on show. Writes "<icon> box ...: N pixels of
# 1,1,1" per icon to NAME.seen, and keeps the last screenshot as NAME.xwd when a frame never showed.
count_keyed() {
    local name=$1 shot=/tmp/keyed.xwd i icon found
    xdotool search --name '^StreamFlex$' windowmove %@ 0 0 windowsize %@ 1920 1080 > /dev/null 2>&1
    for i in $(seq 50); do
        : > "$out/$name.seen"
        found=yes
        xwd -root -silent -out "$shot" || found=no
        for icon in rgba:0,200,200 rgb:0,200,100 svg:0,100,200; do
            echo "${icon%%:*} $(python3 "$HERE/pixels.py" "$shot" count 1,1,1 inside "${icon#*:}" 2>&1)" >> "$out/$name.seen"
            grep -q "^${icon%%:*} box " "$out/$name.seen" || found=no
        done
        [ "$found" = yes ] && return
        sleep 0.2
    done
    cp "$shot" "$out/$name.xwd" 2> /dev/null
}
run_keys f70-chroma +count_keyed
seen=$out/f70-chroma.seen
ok=1
grep -qE '^rgba box .*: 0 pixels of 1,1,1$' "$seen" && grep -qE '^rgb box .*: 0 pixels of 1,1,1$' "$seen" \
    && grep -qE '^svg box .*: 0 pixels of 1,1,1$' "$seen" \
    && grep -q '/home/tester/keyed/rgba.png: moved [0-9]* pixel(s) off the chroma key #010101' "$out/f70-chroma.log" \
    && grep -q '/home/tester/keyed/rgb.png: moved [0-9]* pixel(s) off the chroma key #010101' "$out/f70-chroma.log" \
    && grep -q '/home/tester/keyed/svg.svg: moved [0-9]* pixel(s) off the chroma key #010101' "$out/f70-chroma.log" \
    && ran_clean f70-chroma && ok=0
result "Transparent: no icon's pixel is the chroma key colour, PNG or SVG (exit $(cat "$out/f70-chroma.code"))" $ok
sed 's/^/      /' "$out/f70-chroma.seen"
