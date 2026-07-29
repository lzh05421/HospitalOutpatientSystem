#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DESKTOP_DIR=""

if command -v xdg-user-dir >/dev/null 2>&1; then
    DESKTOP_DIR="$(xdg-user-dir DESKTOP)"
fi

if [[ -z "$DESKTOP_DIR" || "$DESKTOP_DIR" == "$HOME" ]]; then
    if [[ -d "$HOME/桌面" ]]; then
        DESKTOP_DIR="$HOME/桌面"
    else
        DESKTOP_DIR="$HOME/Desktop"
    fi
fi

EXTRA_DESKTOP_DIR=""
if [[ "$DESKTOP_DIR" != "$HOME/桌面" && -d "$HOME/桌面" ]]; then
    EXTRA_DESKTOP_DIR="$HOME/桌面"
elif [[ "$DESKTOP_DIR" != "$HOME/Desktop" && -d "$HOME/Desktop" ]]; then
    EXTRA_DESKTOP_DIR="$HOME/Desktop"
fi

mkdir -p "$DESKTOP_DIR"

SERVER_BINARY="./build-linux/server/hospital_server"
CLIENT_BINARY="./build-linux/client/hospital_client"

if [[ ! -x "$PROJECT_ROOT/$SERVER_BINARY" && -x "$PROJECT_ROOT/build-kylin/server/hospital_server" ]]; then
    SERVER_BINARY="./build-kylin/server/hospital_server"
fi

if [[ ! -x "$PROJECT_ROOT/$CLIENT_BINARY" && -x "$PROJECT_ROOT/build-kylin/client/hospital_client" ]]; then
    CLIENT_BINARY="./build-kylin/client/hospital_client"
fi

escape_for_shell_single_quotes() {
    printf "%s" "$1" | sed "s/'/'\\\\''/g"
}

PROJECT_ROOT_ESCAPED="$(escape_for_shell_single_quotes "$PROJECT_ROOT")"

SERVER_LAUNCHER="$DESKTOP_DIR/Hospital Server.desktop"
CLIENT_LAUNCHER="$DESKTOP_DIR/Hospital Client.desktop"

cat > "$SERVER_LAUNCHER" <<EOF
[Desktop Entry]
Type=Application
Name=Hospital Server
Comment=Start the hospital outpatient system server
Exec=bash -lc "cd '$PROJECT_ROOT_ESCAPED' && $SERVER_BINARY config/server.linux.example.ini"
Terminal=true
Categories=Development;
EOF

cat > "$CLIENT_LAUNCHER" <<EOF
[Desktop Entry]
Type=Application
Name=Hospital Client
Comment=Start the hospital outpatient system client
Exec=bash -lc "cd '$PROJECT_ROOT_ESCAPED' && $CLIENT_BINARY"
Terminal=false
Categories=Office;
EOF

chmod +x "$SERVER_LAUNCHER" "$CLIENT_LAUNCHER"

if [[ -n "$EXTRA_DESKTOP_DIR" ]]; then
    cp "$SERVER_LAUNCHER" "$EXTRA_DESKTOP_DIR/Hospital Server.desktop"
    cp "$CLIENT_LAUNCHER" "$EXTRA_DESKTOP_DIR/Hospital Client.desktop"
    chmod +x "$EXTRA_DESKTOP_DIR/Hospital Server.desktop" "$EXTRA_DESKTOP_DIR/Hospital Client.desktop"
fi

echo "Created: $SERVER_LAUNCHER"
echo "Created: $CLIENT_LAUNCHER"
if [[ -n "$EXTRA_DESKTOP_DIR" ]]; then
    echo "Created: $EXTRA_DESKTOP_DIR/Hospital Server.desktop"
    echo "Created: $EXTRA_DESKTOP_DIR/Hospital Client.desktop"
fi
