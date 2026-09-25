# Tiny Chess

A tiny terminal chess game in C for Omarchy / Linux and Apple Silicon macOS. Mouse and keyboard controls,
local two-player games, and online play. One native executable under 512 KB;
only system C/POSIX libraries, with no GUI libraries or bundled assets.

## Start a game

Run this on your machine. It installs or updates if needed, then hosts on TCP
port 5555. You play White. **Your friend's install-and-connect command is copied
to your clipboard automatically**, with your detected IP and port filled in.
Paste it into a message to your friend; they paste it into their terminal.

```sh
sh -c "$(curl -fsSL https://raw.githubusercontent.com/pavelbohovin/tiny-chess/v1.2.0/install.sh)" -- --host 5555
```

## Give this command to your friend

The command copied when you host looks like this, with `HOST_IP` already filled in.
Your friend plays Black. You can also fill in the address yourself:

```sh
sh -c "$(curl -fsSL https://raw.githubusercontent.com/pavelbohovin/tiny-chess/v1.2.0/install.sh)" -- --join HOST_IP 5555
```

Automatic detection uses your default network route, or an active LAN interface
when there is no default route. It does not contact an IP lookup service. Your
friend must be able to reach that address: normally, use the same LAN or a VPN.
For a different VPN address or a public address, override the copied address:

```sh
./tiny-chess --host 5555 --share-host YOUR_VPN_OR_PUBLIC_IP
```

For internet connections without a VPN, forward TCP port 5555 to the host.
A VM's private address may need VM port forwarding too. Automatic detection
cannot configure routers or determine which private network your friend can reach.

Clipboard copying uses `pbcopy` on macOS, `wl-copy` on Wayland (included in
Omarchy), or `xclip`/`xsel` on X11. The full command is also printed before the
board, available in terminal scrollback after quitting. If clipboard access is
unavailable, the game still works and tells you to copy it manually. Use
`--no-clipboard` to keep your clipboard unchanged. Local games and joining a
friend's game never change the clipboard.

The installer supports **macOS 11+ on Apple Silicon** (M1 and newer), and Linux
x86-64 / ARM64 with glibc 2.28 or newer. Mac and Linux players can play each other.
It checks
the download's SHA-256 checksum and installs to `~/.local/bin/tiny-chess`, updating
older versions and reusing this release on later runs. Uses the system's
`sha256sum` or macOS `shasum`; no Homebrew,
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
