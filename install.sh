#!/bin/sh
# Install the matching Linux release if absent, then run it with these arguments.
set -eu

install_dir=${TINY_CHESS_INSTALL_DIR:-"$HOME/.local/bin"}
binary=$install_dir/tiny-chess

install_game() (
    case $(uname -s) in
        Linux) ;;
        *) echo 'tiny-chess requires Linux (or WSL on Windows).' >&2; exit 1 ;;
    esac
    case $(uname -m) in
        x86_64|amd64) arch=x86_64 ;;
        aarch64|arm64) arch=aarch64 ;;
        *) echo 'No release for this CPU; build chess.c with make instead.' >&2; exit 1 ;;
    esac
    for tool in curl sha256sum awk; do
        command -v "$tool" >/dev/null 2>&1 || {
            echo "Missing command: $tool" >&2
            exit 1
        }
    done

    asset=tiny-chess-linux-$arch
    release=https://github.com/pavelbohovin/tiny-chess/releases/download/v1.0.0
    mkdir -p "$install_dir"
    temp_dir=$(mktemp -d "$install_dir/.tiny-chess.XXXXXX")
    trap 'rm -rf "$temp_dir"' 0
    trap 'exit 1' HUP INT TERM

    echo "Installing tiny-chess for Linux $arch..." >&2
    curl --proto '=https' --tlsv1.2 -fsSL --retry 2 --connect-timeout 15 --max-time 120 \
        "$release/$asset" -o "$temp_dir/$asset"
    curl --proto '=https' --tlsv1.2 -fsSL --retry 2 --connect-timeout 15 --max-time 120 \
        "$release/SHA256SUMS" -o "$temp_dir/SHA256SUMS"
    awk -v asset="$asset" '$2 == asset { print; found++ } END { if (found != 1) exit 1 }' \
        "$temp_dir/SHA256SUMS" > "$temp_dir/checksum"
    (cd "$temp_dir" && sha256sum -c checksum)
    chmod 755 "$temp_dir/$asset"
    "$temp_dir/$asset" --help >/dev/null
    mv -f "$temp_dir/$asset" "$binary"
    echo "Installed: $binary" >&2
)

if [ ! -x "$binary" ]; then
    install_game
fi

if [ "${1:-}" = --install-only ]; then
    printf '%s\n' "$binary"
    exit 0
fi

# Keep the caller's terminal as stdin so mouse input and typed moves both work.
exec "$binary" "$@"
