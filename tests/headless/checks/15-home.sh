# HOME unset, as for a systemd system service with no User= or a launch through `env -i`. The
# launcher used to pass getenv("HOME")'s NULL into its log and config paths and crash before it
# logged anything. Without HOME it asks the user database for the home folder, as a login shell
# does; with no entry there either, it logs to stderr. Both premises are asserted first: tester has
# a home in the user database, and uid 4321 has no entry at all.
tester_home_in_db=$(getent passwd tester | cut -d: -f6)
unknown_in_db=$(getent passwd 4321)

# The launcher's command line with HOME taken out of its environment and, when given, run as that
# user id (with no groups) instead of tester
without_home() {
    local word
    no_home=()
    for word in "${TESTER[@]}"; do
        case $word in
            "HOME=$TESTER_HOME") no_home+=(-u HOME) ;;
            --reuid=tester) no_home+=("--reuid=${1:-tester}") ;;
            --regid=tester) no_home+=("--regid=${1:-tester}") ;;
            --init-groups) [ -n "${1:-}" ] && no_home+=(--clear-groups) || no_home+=(--init-groups) ;;
            *) no_home+=("$word") ;;
        esac
    done
}

# HOME unset: the log goes to the user database's home, and the config search finds the user's
# ~/.config/streamflex/config.ini there. The launcher is a copy with no config beside it, so the
# search reaches that folder.
rm -rf /opt/sf-home "$TESTER_HOME/.config/streamflex"
mkdir -p /opt/sf-home "$TESTER_HOME/.config/streamflex"
cp /work/build/streamflex /opt/sf-home/ && cp -r /work/build/assets /opt/sf-home/
cp "$FX/f15-home.ini" "$TESTER_HOME/.config/streamflex/config.ini"
chown -R tester:tester "$TESTER_HOME/.config"
without_home
( TESTER=("${no_home[@]}"); exe=/opt/sf-home/streamflex CFG=none run_quick f15-nohome )
ok=1
[ "$tester_home_in_db" = "$TESTER_HOME" ] && ran_clean f15-nohome \
    && grep -qx "Config file found: $TESTER_HOME/.config/streamflex/config.ini" "$out/f15-nohome.log" && ok=0
result "HOME unset: the log and the config are found in the user database's home (exit $(cat "$out/f15-nohome.code"))" $ok
[ "$tester_home_in_db" = "$TESTER_HOME" ] || echo "      tester's home in the user database is '$tester_home_in_db', not $TESTER_HOME: fix the premise"
grep -m3 -E 'AddressSanitizer|runtime error' "$out/f15-nohome.err" | sed 's/^/      /'
rm -rf /opt/sf-home "$TESTER_HOME/.config/streamflex"

# HOME unset and a user id the user database does not know: there is no home to keep a log in, so
# it goes to stderr, and the launcher runs as usual
without_home 4321
( TESTER=("${no_home[@]}"); run_quick f15-nouser )
ok=1
[ -z "$unknown_in_db" ] && ran_clean f15-nouser \
    && grep -q '^No home folder: HOME is not set and the user database has no entry for this user, so the log goes to stderr$' "$out/f15-nouser.err" \
    && grep -q "^Loading menu 'Main'" "$out/f15-nouser.err" && ok=0
result "HOME unset and no user database entry: the log goes to stderr (exit $(cat "$out/f15-nouser.code"))" $ok
[ -z "$unknown_in_db" ] || echo "      uid 4321 has an entry in the user database: fix the premise"
grep -m3 -E 'AddressSanitizer|runtime error' "$out/f15-nouser.err" | sed 's/^/      /'
