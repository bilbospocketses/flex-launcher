# Item 22: Xvfb reports a refresh rate of 0, as some VMs and remote desktops do. The launcher
# must start, use 60 Hz and say so, instead of dividing by zero. Whether the 0 reaches it depends
# on the SDL2 underneath: SDL2 passes it on, while sdl2-compat (SDL2's API over SDL3) reports 60 Hz
# for any display that says 0, so there the launcher must use the 60 it is given, and not claim a
# fallback it never made. What this SDL2 reports is asked, through the library the launcher uses.
sdl_hz=$(python3 - 2> "$out/f22-sdl.err" << 'EOF'
import ctypes
sdl = ctypes.CDLL("libSDL2-2.0.so.0")
class DisplayMode(ctypes.Structure):
    _fields_ = [("format", ctypes.c_uint32), ("w", ctypes.c_int), ("h", ctypes.c_int),
                ("refresh_rate", ctypes.c_int), ("driverdata", ctypes.c_void_p)]
mode = DisplayMode()
if sdl.SDL_Init(0x20) != 0 or sdl.SDL_GetDesktopDisplayMode(0, ctypes.byref(mode)) != 0:
    raise SystemExit(1)
print(mode.refresh_rate)
sdl.SDL_Quit()
EOF
)
run_quick f22-refresh
ok=1
if ran_clean f22-refresh && grep -qx 'Refresh rate:  60 Hz' "$out/f22-refresh.log"; then
    case $sdl_hz in
        0) grep -q 'The display reports no refresh rate, using 60 Hz' "$out/f22-refresh.log" && ok=0 ;;
        60) grep -q 'reports no refresh rate' "$out/f22-refresh.log" || ok=0 ;;
    esac
fi
result "item 22: a display that reports 0 Hz starts at 60 Hz (SDL says ${sdl_hz:-nothing} Hz, exit $(cat "$out/f22-refresh.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f22-refresh.err" | sed 's/^/      /'
