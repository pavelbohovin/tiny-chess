#!/bin/sh
# Install/update to this release if needed, then run with these arguments.
set -eu

install_dir=${TINY_CHESS_INSTALL_DIR:-"$HOME/.local/bin"}
binary=$install_dir/tiny-chess

install_game() (
    case $(uname -s) in
        Linux) platform=linux ;;
        Darwin) platform=macos ;;
        *) echo 'tiny-chess requires Linux or Apple Silicon macOS.' >&2; exit 1 ;;
    esac
    case $(uname -m) in
        x86_64|amd64) arch=x86_64 ;;
        aarch64|arm64) arch=aarch64 ;;
        *) echo 'No release for this CPU; build chess.c with make instead.' >&2; exit 1 ;;
    esac
    if [ "$platform" = macos ]; then
        # A Terminal running under Rosetta can report x86_64 on Apple Silicon.
        if [ "$arch" = x86_64 ] && [ "$(sysctl -n hw.optional.arm64 2>/dev/null || echo 0)" != 1 ]; then
            echo 'The macOS download requires Apple Silicon; Intel Macs can build from source.' >&2
            exit 1
        fi
        arch=arm64
    fi
    for tool in curl awk; do
        command -v "$tool" >/dev/null 2>&1 || {
            echo "Missing command: $tool" >&2
            exit 1
        }
    done
    if command -v sha256sum >/dev/null 2>&1; then
        checksum_tool=sha256sum
    elif command -v shasum >/dev/null 2>&1; then
        checksum_tool=shasum
    else
        echo 'Missing checksum tool: need sha256sum or shasum.' >&2
        exit 1
    fi

    asset=tiny-chess-$platform-$arch
    release=https://github.com/pavelbohovin/tiny-chess/releases/download/v1.2.0
    mkdir -p "$install_dir"
    temp_dir=$(mktemp -d "$install_dir/.tiny-chess.XXXXXX")
    trap 'rm -rf "$temp_dir"' 0
    trap 'exit 1' HUP INT TERM

    echo "Installing tiny-chess for $platform $arch..." >&2
    curl --proto '=https' --tlsv1.2 -fsSL --retry 2 --connect-timeout 15 --max-time 120 \
        "$release/$asset" -o "$temp_dir/$asset"
    curl --proto '=https' --tlsv1.2 -fsSL --retry 2 --connect-timeout 15 --max-time 120 \
        "$release/SHA256SUMS" -o "$temp_dir/SHA256SUMS"
    awk -v asset="$asset" '$2 == asset { print; found++ } END { if (found != 1) exit 1 }' \
        "$temp_dir/SHA256SUMS" > "$temp_dir/checksum"
    if [ "$checksum_tool" = shasum ]; then
        (cd "$temp_dir" && shasum -a 256 -c checksum)
    else
        (cd "$temp_dir" && sha256sum -c checksum)
    fi
    chmod 755 "$temp_dir/$asset"
    "$temp_dir/$asset" --help >/dev/null
    mv -f "$temp_dir/$asset" "$binary"
    echo "Installed: $binary" >&2
)

if [ ! -x "$binary" ] || [ "$("$binary" --version 2>/dev/null || :)" != 'tiny-chess 1.2.0' ]; then
    install_game
fi

if [ "${1:-}" = --install-only ]; then
    printf '%s\n' "$binary"
    exit 0
fi

# Keep the caller's terminal as stdin so mouse input and typed moves both work.
exec "$binary" "$@"
