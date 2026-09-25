# Tiny Chess

A tiny terminal chess game in C for Omarchy / Linux. Mouse and keyboard controls,
local two-player games, and online play. One native executable under 512 KB;
only libc, with no GUI libraries or bundled assets.

## Start a game

Run this on your machine. It installs if needed, then hosts on TCP port 5555.
You play White.

```sh
sh -c "$(curl -fsSL https://raw.githubusercontent.com/pavelbohovin/tiny-chess/v1.0.0/install.sh)" -- --host 5555
```

## Give this command to your friend

Replace `HOST_IP` with your reachable IPv4 address or hostname. Your friend plays Black.

```sh
sh -c "$(curl -fsSL https://raw.githubusercontent.com/pavelbohovin/tiny-chess/v1.0.0/install.sh)" -- --join HOST_IP 5555
```

Use the same LAN or a VPN. For internet connections without a VPN, forward TCP
port 5555 to the host and share the public address. `hostname -I` shows your local
addresses; a VM's private address may require VM port forwarding too.

The installer supports Linux x86-64 and ARM64 with glibc 2.28 or newer. It checks
the download's SHA-256 checksum and installs to `~/.local/bin/tiny-chess`, reusing
it on later runs. Requires `curl` and standard Linux utilities; no compiler or sudo.
Both commands keep your terminal connected, so mouse controls work.

## Controls

- Click a piece, then its destination; Esc or right-click cancels.
- Or type `e2 e4` and press Enter.
- Promote with `e7 e8 q/r/b/n` or click a promotion choice.
- `q` quits, `r` restarts, `a` toggles ASCII pieces; press Enter. Online, only the host restarts.

Castling, en passant, check, checkmate, and stalemate are supported. The host
validates moves and keeps both boards in sync. Without `--host` or `--join`, both
players share one terminal.

## Build from source

```sh
git clone https://github.com/pavelbohovin/tiny-chess.git
cd tiny-chess
make
./tiny-chess --host 5555
```

`make` uses GCC size optimization, unused-section removal, and stripping.
Run `./tiny-chess --help` for all options.
