#!/bin/bash
# StreamFlex headless checks. Builds the launcher with ASan and UBSan, starts a virtual X display,
# runs fixture configs (some with key presses) and reads the debug log and the files they write.
# Runs inside the image built from this folder's Dockerfile, with the repo mounted read-only at
# /src and an output folder at /out:
#   run.sh <label>              every check in checks/, in name order
#   run.sh <label> scrollfail   item 11 only: the scroll arrow's texture is forced to fail,
#                               a path no config or input can reach
# Prints PASS or FAIL per check and exits non-zero when any failed. Every run's output, log and
# exit code are kept in /out/<label>.
set -u
label=$1
fault=${2:-}
HERE=/src/tests/headless
FX=$HERE/fixtures
out=/out/$label
rm -rf "$out"; mkdir -p "$out"

# Build a copy of the source, without the Windows build tree (vcpkg, several GB) or .git
rm -rf /work; mkdir -p /work
tar -C /src --exclude=./build --exclude=./.git -cf - . | tar -C /work -xf -

if [ "$fault" = scrollfail ]; then
    sed -i 's|^    if (scroll->texture == NULL)|    SDL_DestroyTexture(scroll->texture); scroll->texture = NULL; /* FAULT */\n&|' /work/src/image.c
    grep -q 'FAULT' /work/src/image.c || { echo "FAULT NOT INJECTED"; exit 2; }
fi

flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
cmake -S /work -B /work/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS="$flags" \
      -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" > "$out/configure.log" 2>&1 \
    || { echo "CONFIGURE FAILED"; tail -20 "$out/configure.log"; exit 2; }
cmake --build /work/build -j"$(nproc)" > "$out/build.log" 2>&1 \
    || { echo "BUILD FAILED"; tail -30 "$out/build.log"; exit 2; }
grep -iE 'warning:' "$out/build.log" | sed 's/^/BUILD WARNING: /'

# Xvfb reports a refresh rate of 0, which the launcher must survive (item 22). -ac lets the
# unprivileged test user connect to it.
Xvfb :99 -screen 0 1920x1080x24 -ac > /dev/null 2>&1 &
export DISPLAY=:99
for i in $(seq 100); do [ -S /tmp/.X11-unix/X99 ] && break; sleep 0.2; done
[ -S /tmp/.X11-unix/X99 ] || { echo "Xvfb DID NOT START"; exit 2; }
sleep 1

# The launcher runs as `tester`: root ignores file permissions, and the settings checks need a
# config it cannot write. setpriv, env and setarch each exec the next, so the launcher keeps the
# PID the shell sees. setarch -R turns address randomization off, because GCC 12's ASan crashes
# at random when the kernel randomizes 32 bits of mmap (WSL2, and GitHub's ubuntu-24.04
# runners); it needs the container started with --security-opt seccomp=unconfined. Mesa's
# softpipe has no JIT; llvmpipe's JIT made ASan runs crash at random.
exe=/work/build/streamflex
TESTER_HOME=/home/tester
LOG=$TESTER_HOME/.local/share/streamflex/streamflex.log
TESTER=(setpriv --reuid=tester --regid=tester --init-groups --
        env HOME=$TESTER_HOME DISPLAY=:99 ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1
            GALLIUM_DRIVER=softpipe setarch "$(uname -m)" -R)

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

# A config left running: send the keys a second apart, then quit it with SIGTERM (SDL turns
# that into a quit event)
run_keys() {
    local name=$1; shift
    local args; mapfile -t args < <(config_args "$name")
    rm -f "$LOG"
    "${TESTER[@]}" "$exe" "${args[@]}" -d > "$out/$name.out" 2> "$out/$name.err" &
    local pid=$!
    sleep 4
    for k in "$@"; do xdotool key "$k"; sleep 1; done
    kill -TERM "$pid" 2> /dev/null; wait "$pid"; echo $? > "$out/$name.code"
    cp "$LOG" "$out/$name.log" 2> /dev/null || : > "$out/$name.log"
}

sanitizer_clean() { ! grep -qE 'AddressSanitizer|runtime error|LeakSanitizer' "$out/$1.err"; }

failures=0
result() {
    if [ "$2" = 0 ]; then echo "PASS  $1"; else echo "FAIL  $1"; failures=$((failures + 1)); fi
}

if [ "$fault" = scrollfail ]; then
    run_quick f11-scroll
    ok=1
    [ "$(cat "$out/f11-scroll.code")" = 0 ] && sanitizer_clean f11-scroll \
        && grep -q 'Could not render scroll indicator' "$out/f11-scroll.log" && ok=0
    result "item 11: a failed scroll arrow disables the arrows and exits cleanly (exit $(cat "$out/f11-scroll.code"))" $ok
    grep -m3 -E 'AddressSanitizer|double-free|runtime error' "$out/f11-scroll.err" | sed 's/^/      /'
else
    for check in "$HERE"/checks/*.sh; do
        . "$check"
    done
fi
echo "$failures failed"
[ "$failures" = 0 ]
