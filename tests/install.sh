#!/bin/sh
set -eu
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d "${TMPDIR:-/tmp}/tiny-chess-install-test.XXXXXX")
trap 'rm -rf "$work"' 0
mkdir -p "$work/mock" "$work/assets" "$work/bin with spaces"
cat > "$work/mock/uname" <<'MOCK'
#!/bin/sh
case "$1" in -s) echo "${TEST_OS:-Linux}" ;; -m) echo "${TEST_ARCH:-aarch64}" ;; esac
MOCK
cat > "$work/mock/sysctl" <<'MOCK'
#!/bin/sh
echo "${TEST_ARM_CAPABLE:-0}"
MOCK
cat > "$work/mock/curl" <<'MOCK'
#!/bin/sh
set -eu
printf 'download\n' >> "$TEST_WORK/downloads"
[ "${TEST_FAIL_DOWNLOAD:-0}" = 0 ] || exit 22
while [ "$#" -gt 0 ]; do
    case "$1" in
        https://*) asset=${1##*/} ;;
        -o) shift; output=$1 ;;
    esac
    shift
done
cp "$TEST_WORK/assets/$asset" "$output"
MOCK
cat > "$work/assets/tiny-chess-linux-aarch64" <<'MOCK'
#!/bin/sh
set -eu
if [ "${1:-}" = --help ]; then exit 0; fi
if [ "${1:-}" = --version ]; then echo 'tiny-chess 1.2.0'; exit 0; fi
printf '%s\n' "$@" > "$TEST_WORK/arguments"
if [ "${TEST_READ_STDIN:-0}" = 1 ]; then
    IFS= read -r input
    printf '%s\n' "$input" > "$TEST_WORK/input"
fi
exit "${TEST_EXIT:-0}"
MOCK
cp "$work/assets/tiny-chess-linux-aarch64" "$work/assets/tiny-chess-linux-x86_64"
cp "$work/assets/tiny-chess-linux-aarch64" "$work/assets/tiny-chess-macos-arm64"
chmod +x "$work/mock/"* "$work/assets/"*
(cd "$work/assets" && shasum -a 256 tiny-chess-* > SHA256SUMS)
PATH="$work/mock:$PATH"; export PATH
TEST_WORK=$work; export TEST_WORK
TINY_CHESS_INSTALL_DIR="$work/bin with spaces"; export TINY_CHESS_INSTALL_DIR
sh -n "$project/install.sh"
printf 'stdin preserved\n' | TEST_READ_STDIN=1 sh "$project/install.sh" --join host.example 5555
printf '%s\n' --join host.example 5555 > "$work/expected"
cmp "$work/arguments" "$work/expected"
test "$(cat "$work/input")" = 'stdin preserved'
test -x "$TINY_CHESS_INSTALL_DIR/tiny-chess"
test "$(wc -l < "$work/downloads")" -eq 2
TEST_FAIL_DOWNLOAD=1 sh "$project/install.sh" --join 'host with spaces' 9999
printf '%s\n' --join 'host with spaces' 9999 > "$work/expected"
cmp "$work/arguments" "$work/expected"
test "$(wc -l < "$work/downloads")" -eq 2
if TEST_EXIT=7 sh "$project/install.sh" --join host 5555; then exit 1; else test "$?" -eq 7; fi
TEST_FAIL_DOWNLOAD=1 sh "$project/install.sh" --install-only > "$work/install-path"
test "$(cat "$work/install-path")" = "$TINY_CHESS_INSTALL_DIR/tiny-chess"
echo 'Install, reuse, arguments, stdin, exit status: PASS'
# Older releases did not understand --version; replace only after a verified download.
printf '#!/bin/sh\nexit 1\n' > "$TINY_CHESS_INSTALL_DIR/tiny-chess"
if TEST_FAIL_DOWNLOAD=1 sh "$project/install.sh" --install-only; then exit 1; fi
test -x "$TINY_CHESS_INSTALL_DIR/tiny-chess"
if "$TINY_CHESS_INSTALL_DIR/tiny-chess" --version; then exit 1; fi
sh "$project/install.sh" --install-only
test "$("$TINY_CHESS_INSTALL_DIR/tiny-chess" --version)" = 'tiny-chess 1.2.0'
echo 'Upgrade old release; failed upgrade preserves existing executable: PASS'
TINY_CHESS_INSTALL_DIR="$work/x86" TEST_ARCH=x86_64 sh "$project/install.sh" --install-only
TINY_CHESS_INSTALL_DIR="$work/arm" TEST_ARCH=arm64 sh "$project/install.sh" --install-only
TINY_CHESS_INSTALL_DIR="$work/mac" TEST_OS=Darwin TEST_ARCH=arm64 sh "$project/install.sh" --install-only
TINY_CHESS_INSTALL_DIR="$work/rosetta" TEST_OS=Darwin TEST_ARCH=x86_64 TEST_ARM_CAPABLE=1 sh "$project/install.sh" --install-only
if TINY_CHESS_INSTALL_DIR="$work/intel-mac" TEST_OS=Darwin TEST_ARCH=x86_64 sh "$project/install.sh"; then exit 1; fi
if TINY_CHESS_INSTALL_DIR="$work/unsupported" TEST_ARCH=riscv64 sh "$project/install.sh"; then exit 1; fi
if TINY_CHESS_INSTALL_DIR="$work/windows" TEST_OS=Windows sh "$project/install.sh"; then exit 1; fi
test ! -e "$work/unsupported/tiny-chess"
echo 'Linux and Apple Silicon detection, Rosetta, unsupported platforms: PASS'

# Exercise macOS's shasum path even on Linux, where sha256sum is normally found.
mkdir -p "$work/shasum-only"
for tool in awk mkdir mktemp rm chmod mv cp; do
    ln -s "$(command -v "$tool")" "$work/shasum-only/$tool"
done
for tool in curl uname sysctl; do ln -s "$work/mock/$tool" "$work/shasum-only/$tool"; done
TEST_SHASUM=$(command -v shasum); export TEST_SHASUM
cat > "$work/shasum-only/shasum" <<'MOCK'
#!/bin/sh
printf 'shasum\n' >> "$TEST_WORK/checker"
exec "$TEST_SHASUM" "$@"
MOCK
chmod +x "$work/shasum-only/shasum"
PATH="$work/shasum-only" TINY_CHESS_INSTALL_DIR="$work/check-mac" TEST_OS=Darwin TEST_ARCH=arm64 /bin/sh "$project/install.sh" --install-only
test "$(cat "$work/checker")" = shasum
echo 'macOS checksum verification without GNU tools: PASS'

if TINY_CHESS_INSTALL_DIR="$work/failure" TEST_FAIL_DOWNLOAD=1 sh "$project/install.sh"; then exit 1; fi
test ! -e "$work/failure/tiny-chess"
test -z "$(ls -A "$work/failure")"
printf 'tampered\n' >> "$work/assets/tiny-chess-linux-aarch64"
if TINY_CHESS_INSTALL_DIR="$work/corrupt" sh "$project/install.sh"; then exit 1; fi
test ! -e "$work/corrupt/tiny-chess"
test -z "$(ls -A "$work/corrupt")"
echo 'Download and checksum failures clean up without installing: PASS'
echo 'ALL INSTALLER TESTS PASSED'
