# Tiny Chess

A tiny terminal chess game in C for Omarchy / Linux and Apple Silicon macOS. Mouse and keyboard controls,
local two-player games, and online play. One native executable under 512 KB;
only system C/POSIX libraries, with no GUI libraries or bundled assets.

## Start a game

Run this on your machine. It installs if needed, then hosts on TCP port 5555.
You play White.

```sh
sh -c "$(curl -fsSL https://raw.githubusercontent.com/pavelbohovin/tiny-chess/v1.1.0/install.sh)" -- --host 5555
```

## Give this command to your friend

Replace `HOST_IP` with your reachable IPv4 address or hostname. Your friend plays Black.

```sh
sh -c "$(curl -fsSL https://raw.githubusercontent.com/pavelbohovin/tiny-chess/v1.1.0/install.sh)" -- --join HOST_IP 5555
```

Use the same LAN or a VPN. For internet connections without a VPN, forward TCP
port 5555 to the host and share the public address. On Linux, `hostname -I` shows
your local addresses. On macOS, find the IP in System Settings → Network → your
connection → Details → TCP/IP. A VM's private address may need VM port forwarding too.

The installer supports **macOS 11+ on Apple Silicon** (M1 and newer), and Linux
x86-64 / ARM64 with glibc 2.28 or newer. Mac and Linux players can play each other.
It checks
the download's SHA-256 checksum and installs to `~/.local/bin/tiny-chess`, reusing
it on later runs. Uses the system's `sha256sum` or macOS `shasum`; no Homebrew,
compiler, or sudo needed. On macOS, open **Terminal**, paste the command, and press Return.
Both commands keep your terminal connected, so mouse controls work.

## Controls

- Click a piece, then its destination; Esc or right-click cancels.
- Or type `e2 e4` and press Enter.
- Promote with `e7 e8 q/r/b/n` or click a promotion choice.
- `q` quits, `r` restarts, `a` toggles ASCII pieces; press Enter. Online, only the host restarts.

In macOS Terminal, keep **View → Allow Mouse Reporting** enabled for clicks
([Apple's instructions](https://support.apple.com/guide/terminal/trmlc69728a5/mac)).
If macOS asks to allow incoming connections while hosting, allow them so your friend can join.

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

`make` uses your system C compiler, size optimization, unused-section removal,
and stripping. On macOS, source builds need Apple's Command Line Tools
(`xcode-select --install`); downloaded releases do not. Mac builds also receive
a local code signature. Run `make test` for rule, mouse, terminal, network, and
installer checks, or `./tiny-chess --help` for all options.
