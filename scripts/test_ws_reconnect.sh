#!/usr/bin/env bash
set -uo pipefail

HOST="${1:-esp32-client.local}"
N="${2:-30}"
WS_URL="ws://${HOST}/ws"
PASS=0
FAIL=0

# ── Resolve websocat binary once (avoid flake re-evaluation per iteration) ───
echo "Resolving websocat..."
WEBSOCAT=$(nix build --no-link --print-out-paths nixpkgs#websocat 2>/dev/null)/bin/websocat
if [ ! -x "$WEBSOCAT" ]; then
    echo "ERROR: could not build websocat via nix"
    exit 1
fi

# Connect to WS, wait for settings push (ESP32 sends after 100ms timer),
# then let stdin close -> websocat sends proper WS CLOSE -> clean TCP shutdown.
# Using a temp file avoids head-in-pipe (SIGPIPE -> RST) which causes socket
# slot exhaustion on the ESP32 side.
ws_recv_settings() {
    local url="$1" err_file="$2"
    local msgfile
    msgfile=$(mktemp)
    # sleep 0.3 keeps stdin open long enough for the 100ms push timer to fire,
    # then EOF -> websocat sends WS CLOSE frame -> ESP32 frees socket cleanly.
    (sleep 0.3; true) | timeout 5 "$WEBSOCAT" "$url" 2>"$err_file" > "$msgfile" || true
    head -n1 "$msgfile"
    rm -f "$msgfile"
}

# ── Optional serial log monitoring ───────────────────────────────────────────
SERIAL_LOG=""
SERIAL_PID=""
for port in /dev/ttyACM0 /dev/ttyUSB0 /dev/ttyACM1; do
    if [ -e "$port" ] && [ -r "$port" ]; then
        SERIAL_LOG=$(mktemp)
        stty -F "$port" 115200 raw -echo 2>/dev/null
        cat "$port" >> "$SERIAL_LOG" &
        SERIAL_PID=$!
        echo "Serial: $port -> $SERIAL_LOG"
        break
    fi
done
trap '[ -n "$SERIAL_PID" ] && kill "$SERIAL_PID" 2>/dev/null; true' EXIT

# ── Pre-flight: layered connectivity check ───────────────────────────────────
echo "Pre-flight checks for $HOST:"

# Level 1: DNS/mDNS resolution (skip if HOST is already an IP)
if echo "$HOST" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$'; then
    IP="$HOST"
    echo "  IP: $HOST (direct)"
else
    IP=$(getent hosts "$HOST" 2>/dev/null | awk '{print $1}')
    if [ -z "$IP" ]; then
        IP=$(avahi-resolve-host-name "$HOST" 2>/dev/null | awk '{print $2}')
    fi
    if [ -z "$IP" ]; then
        echo "  FAIL: cannot resolve $HOST"
        echo "  Try: avahi-browse -art | grep -i esp"
        echo "  Or pass IP directly: $0 192.168.x.x"
        exit 1
    fi
    echo "  DNS: $HOST -> $IP"
    WS_URL="ws://${IP}/ws"
fi

# Level 2: HTTP GET / (checks TCP connect + HTTP in one step)
HTTP=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 5 --max-time 8 \
           -H "Connection: close" "http://$IP/" 2>/dev/null)
HTTP="${HTTP:-000}"
if [ "$HTTP" = "000" ]; then
    echo "  FAIL: HTTP http://$IP/ not reachable"
    echo "  Is loc_server_start() called on ESP32?"
    exit 1
fi
echo "  HTTP: $HTTP"

# Level 3: WebSocket (clean close via ws_recv_settings)
ERR_LOG=$(mktemp)
TEST_MSG=$(ws_recv_settings "$WS_URL" "$ERR_LOG")
if ! echo "$TEST_MSG" | grep -q '"type"'; then
    echo "  FAIL: WebSocket $WS_URL"
    echo "  websocat: $(cat "$ERR_LOG")"
    exit 1
fi
echo "  WS: OK (got $(echo "$TEST_MSG" | grep -o '"type":"[^"]*"'))"
echo ""

# ── Main reconnect loop ───────────────────────────────────────────────────────
echo "Testing $N reconnections -> $WS_URL"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

ERR=$(mktemp)
for i in $(seq 1 "$N"); do
    MSG=$(ws_recv_settings "$WS_URL" "$ERR")
    if echo "$MSG" | grep -q '"type":"settings"'; then
        PASS=$((PASS + 1))
        printf "\r[%d/%d] pass=%d fail=%d" "$i" "$N" "$PASS" "$FAIL"
    else
        FAIL=$((FAIL + 1))
        echo ""
        echo "  X iter $i: $(head -1 "$ERR")"
    fi
    sleep 0.2
done

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Done: $PASS passed, $FAIL failed / $N total"

# ── Check serial log for crash ────────────────────────────────────────────────
if [ -n "$SERIAL_LOG" ] && [ -f "$SERIAL_LOG" ]; then
    if grep -q "Guru Meditation\|LoadProhibited\|StoreProhibited\|abort()" "$SERIAL_LOG" 2>/dev/null; then
        echo "CRASH DETECTED in serial log!"
        grep -A5 "Guru Meditation\|LoadProhibited\|abort()" "$SERIAL_LOG"
        exit 1
    fi
    echo "Serial log: no crash"
fi

[ "$FAIL" -eq 0 ]
