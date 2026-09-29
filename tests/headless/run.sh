#!/bin/bash
# StreamFlex headless checks. Builds the launcher with ASan and UBSan, starts a virtual X display,
# runs fixture configs (some with key presses) and reads the debug log, the files they write and
# the screen. Runs inside the image built from this folder's Dockerfile, with the repo mounted
# read-only at /src and an output folder at /out:
#   run.sh <label>              every check in checks/, in name order
#   run.sh <label> scrollfail   item 11 only: the scroll arrow's texture is forced to fail,
#                               a path no config or input can reach
#   run.sh <label> leaks        every check again, with LeakSanitizer on: a run that leaks fails
#                               its check, and the leaks are listed at the end
# The build defines STREAMFLEX_TEST_HOOKS, which only this harness does (see decode_image()).
# Prints PASS or FAIL per check and exits non-zero when any failed. Every run's output, log and
# exit code are kept in /out/<label>.
set -u
label=${1:-}
fault=${2:-}
case $label in
    '' | . | .. | */*) echo "usage: run.sh <label> [scrollfail|leaks], where <label> names a folder in /out"; exit 2 ;;
esac
case $fault in
    '' | scrollfail | leaks) ;;
    *) echo "unknown mode '$fault'"; exit 2 ;;
esac
HERE=/src/tests/headless
FX=$HERE/fixtures
out=/out/$label
rm -rf "$out"; mkdir -p "$out"

# Build a copy of the source, without the Windows build tree (vcpkg, several GB), .git, or the
# output of an earlier run that CONTRIBUTING's command keeps in the repo
rm -rf /work; mkdir -p /work
tar -C /src --exclude=./build --exclude=./.git --exclude=./headless-out -cf - . | tar -C /work -xf -

if [ "$fault" = scrollfail ]; then
    sed -i 's|^    if (scroll->texture == NULL)|    SDL_DestroyTexture(scroll->texture); scroll->texture = NULL; /* FAULT */\n&|' /work/src/image.c
    grep -q 'FAULT' /work/src/image.c || { echo "FAULT NOT INJECTED"; exit 2; }
fi

flags="-fsanitize=address,undefined -fno-omit-frame-pointer -DSTREAMFLEX_TEST_HOOKS"
cmake -S /work -B /work/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="$flags" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" > "$out/configure.log" 2>&1 \
    || { echo "CONFIGURE FAILED"; tail -20 "$out/configure.log"; exit 2; }
cmake --build /work/build -j"$(nproc)" > "$out/build.log" 2>&1 \
    || { echo "BUILD FAILED"; tail -30 "$out/build.log"; exit 2; }

failures=0
result() {
    if [ "$2" = 0 ]; then echo "PASS  $1"; else echo "FAIL  $1"; failures=$((failures + 1)); fi
}

# A compiler warning in our code fails the run; the bundled libraries in src/external are not ours
warnings=$(grep -iE 'warning:' "$out/build.log")
ours=$(printf '%s\n' "$warnings" | grep -v '^/work/src/external/' | sed '/^$/d')
printf '%s\n' "$warnings" | sed '/^$/d; s/^/BUILD WARNING: /'
ok=1; [ -z "$ours" ] && ok=0
result "the build has no compiler warnings outside src/external" $ok

# Xvfb reports a refresh rate of 0, which the launcher must survive (item 22). -ac lets the
# unprivileged test user connect to it.
Xvfb :99 -screen 0 1920x1080x24 -ac > /dev/null 2>&1 &
export DISPLAY=:99
for i in $(seq 100); do xdotool getdisplaygeometry > /dev/null 2>&1 && break; sleep 0.2; done
xdotool getdisplaygeometry > /dev/null 2>&1 || { echo "Xvfb DID NOT START"; exit 2; }

# The launcher runs as `tester`: root ignores file permissions, and the settings checks need a
# config it cannot write. setpriv, env and setarch each exec the next, so the launcher keeps the
# PID the shell sees. setarch -R turns address randomization off, because GCC 12's ASan crashes
# at random when the kernel randomizes 32 bits of mmap (WSL2, and GitHub's ubuntu-24.04
# runners); it needs the container started with --security-opt seccomp=unconfined. Mesa's
# softpipe has no JIT; llvmpipe's JIT made ASan runs crash at random.
exe=/work/build/streamflex
TESTER_HOME=/home/tester
LOG=$TESTER_HOME/.local/share/streamflex/streamflex.log

# Pictures for the background checks: three in Pictures, one on its own, and an empty folder
python3 "$HERE/make_images.py" "$TESTER_HOME/Pictures"
mkdir -p "$TESTER_HOME/one" "$TESTER_HOME/empty"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/one/"
# Slideshow folders that fail: two files that only look like pictures, and one picture beside one
mkdir -p "$TESTER_HOME/broken" "$TESTER_HOME/mixed"
printf 'not a picture\n' > "$TESTER_HOME/broken/a.png"
printf 'not a picture\n' > "$TESTER_HOME/broken/b.png"
cp "$TESTER_HOME/Pictures/red.png" "$TESTER_HOME/mixed/"
printf 'not a picture\n' > "$TESTER_HOME/mixed/broken.png"
chown -R tester:tester "$TESTER_HOME"
# The leak pass preloads Mesa's driver into the launcher. Unpreloaded, libGL unloads it at exit,
# before LeakSanitizer looks, so the few blocks the driver still holds (from context creation and
# its first flush) lose their only pointers and read as leaks, with stacks in an unknown module.
# ASan must come first in the preload list. Only the launcher gets the preload (the env after
# setarch): an ASan runtime in setarch starts before randomization is off, and crashes. Whole
# stacks (fast_unwind_on_malloc=0) reach our code through libraries built without frame
# pointers; that is slower, so only this pass does it. It keeps setarch -R: the ASan build
# crashes at random without it whether or not leaks are looked for.
asan_options=detect_leaks=0
preload=()
if [ "$fault" = leaks ]; then
    asan_options=detect_leaks=1:fast_unwind_on_malloc=0
    libasan=$(readlink -f "$(gcc -print-file-name=libasan.so)")
    driver=/usr/lib/$(gcc -print-multiarch)/dri/swrast_dri.so
    [ -f "$libasan" ] && [ -f "$driver" ] || { echo "NO ASAN RUNTIME OR MESA DRIVER TO PRELOAD"; exit 2; }
    preload=(env "LD_PRELOAD=$libasan $driver")
fi
TESTER=(setpriv --reuid=tester --regid=tester --init-groups --
        env HOME=$TESTER_HOME DISPLAY=:99 ASAN_OPTIONS=$asan_options UBSAN_OPTIONS=print_stacktrace=1
            GALLIUM_DRIVER=softpipe setarch "$(uname -m)" -R "${preload[@]}")

# A function to give the launcher its config: the fixture NAME.ini, the file CFG names, or
# none at all with CFG=none (the launcher then searches for one)
config_args() {
    local cfg=${CFG:-$FX/$1.ini}
    [ "$cfg" = none ] || printf '%s\n' -c "$cfg"
}

# A config whose StartupCmd quits by itself. One that hangs gets TERM after 30 s, and KILL 5 s
# later: SDL turns TERM into a quit event, which a launcher stuck in a loop never reads.
run_quick() {
    local name=$1
    local args; mapfile -t args < <(config_args "$name")
    rm -f "$LOG"
    timeout -k 5 -s TERM 30 "${TESTER[@]}" "$exe" "${args[@]}" -d > "$out/$name.out" 2> "$out/$name.err"
    echo $? > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

# A function to tell whether the process PID is still running (an exited child stays a zombie
# until it is waited for, and kill -0 still finds a zombie)
running() {
    local state
    read -r _ _ state _ 2> /dev/null < "/proc/$1/stat" && [ "$state" != Z ]
}

# A function to wait up to 20 s for a line in the launcher's log. It fails when the line never
# comes, or when the launcher (PID) exits without writing it.
wait_line() {
    local line=$1 pid=$2 i
    for i in $(seq 100); do
        grep -qF -- "$line" "$LOG" 2> /dev/null && return 0
        running "$pid" || break
        sleep 0.2
    done
    grep -qF -- "$line" "$LOG" 2> /dev/null
}

# A function to run a config that keeps running, and drive it. It starts the launcher, waits for
# a line in its log (WAIT_FOR, by default the first "Loading menu"), sends the keys a second
# apart, waits for the line UNTIL when that is set, then quits it with TERM (SDL turns that into
# a quit event) and KILLs it 10 s later (exit 137). A key written +name calls the function
# `name` with the run's name instead: that is how a check does something at a moment the log
# chooses. Every wait is bounded; a line that never came is written into NAME.code beside the
# exit code, so the check's exit code test fails. run_after_line and run_slideshow use this too.
run_keys() {
    local name=$1; shift
    local args; mapfile -t args < <(config_args "$name")
    local start=${WAIT_FOR:-Loading menu} missing="" pid code k i
    rm -f "$LOG" "$out/$name.seen" "$out/$name.pixels"
    "${TESTER[@]}" "$exe" "${args[@]}" -d > "$out/$name.out" 2> "$out/$name.err" &
    pid=$!
    if wait_line "$start" "$pid"; then
        for k in "$@"; do
            case $k in
                +*) "${k#+}" "$name" "$pid" ;;
                *) xdotool key "$k"; sleep 1 ;;
            esac
        done
        [ -z "${UNTIL:-}" ] || wait_line "$UNTIL" "$pid" || missing=$UNTIL
    else
        missing=$start
    fi
    kill -TERM "$pid" 2> /dev/null
    for i in $(seq 50); do running "$pid" || break; sleep 0.2; done
    kill -KILL "$pid" 2> /dev/null
    # A killed run's "Killed" notice goes nowhere: its exit code, 137, says so
    wait "$pid" 2> /dev/null; code=$?
    [ -z "$missing" ] || code="$code, and never logged '$missing'"
    echo "$code" > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

# A function to run a config until its log shows LINE, then send the keys as run_keys does
run_after_line() {
    local name=$1 line=$2; shift 2
    WAIT_FOR=$line run_keys "$name" "$@"
}

# A function to run a slideshow fixture: once its first picture is up, run the rest of the
# arguments as a command (which may take the pictures away), then wait for the loader's verdict,
# the log line UNTIL (the fixtures change every 5 s, the shortest allowed), and quit
run_slideshow() {
    local name=$1 until=$2; shift 2
    slideshow_command=("$@")
    WAIT_FOR='Background set up: Slideshow' UNTIL=$until run_keys "$name" +run_slideshow_command
}
run_slideshow_command() { "${slideshow_command[@]}"; }

# A function to tell whether the sanitizers reported nothing for the run NAME
sanitizer_clean() { ! grep -qE 'AddressSanitizer|runtime error|LeakSanitizer' "$out/$1.err"; }

# A function to tell whether the run NAME exited as expected (0 unless CODE is given) with its
# sanitizers quiet. A launcher that had to be killed exits 137, so it fails here on its own.
ran_clean() { [ "$(cat "$out/$1.code")" = "${2:-0}" ] && sanitizer_clean "$1"; }

# A function to tell whether a log has the line START, a line END after it, and a line holding
# NEEDLE between the two (all fixed strings). A range whose END never comes does not count, so
# a launcher that stopped logging halfway cannot pass by running to the end of the file.
in_range() {
    awk -v s="$2" -v e="$3" -v n="$4" '
        !open && index($0, s) { open = 1; next }
        open && index($0, n) { found = 1 }
        open && index($0, e) { closed = 1; exit }
        END { exit !(closed && found) }
    ' "$1"
}

# A function to map a point of the launcher's scene (the full 1920 x 1080 screen it would draw)
# to the screen, inside the settings preview, whose place the live log gives. It fails when the
# log has not said where the preview is.
preview_point() {
    local rect px py pw ph
    rect=$(grep -o 'Settings: the preview is at [0-9]*,[0-9]*, [0-9]* x [0-9]*' "$LOG" | tail -1)
    read -r px py pw ph <<< "$(sed 's/.* at \([0-9]*\),\([0-9]*\), \([0-9]*\) x \([0-9]*\)/\1 \2 \3 \4/' <<< "$rect")"
    [ -n "${ph:-}" ] || return 1
    echo "$((px + $1 * pw / 1920)),$((py + $2 * ph / 1080))"
}

# A function to wait up to 10 s for the screen to show the colours asked for, each x,y=r,g,b:
# it takes a screenshot, reads the points and tries again until they match. Every reading is
# kept in NAME.pixels under TAG, and the last screenshot in NAME-TAG.xwd when they never match.
screen_shows() {
    local name=$1 tag=$2; shift 2
    local shot=/tmp/screen.xwd i
    for i in $(seq 50); do
        if xwd -root -silent -out "$shot" && { echo "$tag:"; python3 "$HERE/pixels.py" "$shot" "$@"; } \
            >> "$out/$name.pixels" 2>&1; then
            return 0
        fi
        sleep 0.2
    done
    cp "$shot" "$out/$name-$tag.xwd" 2> /dev/null
    return 1
}

# A function for a +key (see run_keys), called with the run's NAME and PID: wait for the log line
# LINE, then for the settings preview to show each colour asked for, written sx,sy=r,g,b with the
# point in the launcher's scene (or @x,y=r,g,b, a point of the screen). Writes "TAG yes" or
# "TAG no" to NAME.seen for the check to read; a preview whose place was never logged is "no".
look() {
    local name=$1 pid=$2 tag=$3 line=$4; shift 4
    local asked=() p rest point placed=yes seen=no
    # Xvfb has no window manager to carry out the launcher's fullscreen request, so its window
    # stays 1 x 1 and nothing it draws reaches the screen: give it the screen, as one would
    xdotool search --name '^StreamFlex$' windowmove %@ 0 0 windowsize %@ 1920 1080 > /dev/null 2>&1
    if wait_line "$line" "$pid"; then
        for p in "$@"; do
            rest=${p#*,}
            case $p in
                @*) asked+=("${p#@}") ;;
                *) point=$(preview_point "${p%%,*}" "${rest%%=*}") || placed=no
                   asked+=("$point=${p#*=}") ;;
            esac
        done
        [ "$placed" = yes ] && screen_shows "$name" "$tag" "${asked[@]}" && seen=yes
        [ "$placed" = yes ] || echo "$tag: the log never said where the preview is" >> "$out/$name.pixels"
    fi
    echo "$tag $seen" >> "$out/$name.seen"
}

if [ "$fault" = scrollfail ]; then
    run_quick f11-scroll
    ok=1
    ran_clean f11-scroll && grep -q 'Could not render scroll indicator' "$out/f11-scroll.log" && ok=0
    result "item 11: a failed scroll arrow disables the arrows and exits cleanly (exit $(cat "$out/f11-scroll.code"))" $ok
    grep -m3 -E 'AddressSanitizer|double-free|runtime error' "$out/f11-scroll.err" | sed 's/^/      /'
else
    # A check file that does not parse would stop part-way through when sourced, and the checks
    # after the error would be missing without a word, so each is parsed first
    shopt -s nullglob
    checks=("$HERE"/checks/*.sh)
    [ "${#checks[@]}" -gt 0 ] || { echo "NO CHECKS FOUND in $HERE/checks"; exit 2; }
    for check in "${checks[@]}"; do
        if bash -n "$check" 2> "$out/parse.err"; then
            . "$check"
        else
            result "$(basename "$check") parses" 1
            sed 's/^/      /' "$out/parse.err"
        fi
    done
    if [ "$fault" = leaks ]; then
        for err in "$out"/*.err; do
            grep -q 'ERROR: LeakSanitizer' "$err" \
                && echo "LEAK  $(basename "$err" .err): $(grep -m1 '^SUMMARY' "$err")"
        done
    fi
fi
echo "$failures failed"
[ "$failures" = 0 ]
