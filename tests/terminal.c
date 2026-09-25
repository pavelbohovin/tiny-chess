#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static int master;
static pid_t child;
static struct termios original;
static char output[65536];

static void collect(void)
{
    size_t n = 0;
    struct pollfd fd = { .fd = master, .events = POLLIN };
    while (poll(&fd, 1, n ? 100 : 2000) > 0) {
        ssize_t got = read(master, output + n, sizeof output - n - 1);
        if (got <= 0) break;
        n += (size_t)got;
        assert(n < sizeof output - 1);
    }
    output[n] = '\0';
}

static void expect(const char *text)
{
    if (!strstr(output, text)) {
        fprintf(stderr, "Expected: %s\nOutput: %s\n", text, output);
        assert(0);
    }
}

static void send_text(const char *text)
{
    assert(write(master, text, strlen(text)) == (ssize_t)strlen(text));
    collect();
}

static void launch(int color)
{
    master = posix_openpt(O_RDWR | O_NOCTTY);
    assert(master >= 0 && !grantpt(master) && !unlockpt(master));
    int slave = open(ptsname(master), O_RDWR | O_NOCTTY);
    assert(slave >= 0 && !tcgetattr(slave, &original));
    struct winsize size = { .ws_row = 24, .ws_col = 80 };
    assert(!ioctl(slave, TIOCSWINSZ, &size));
    child = fork();
    assert(child >= 0);
    if (!child) {
        close(master);
        assert(setsid() >= 0 && !ioctl(slave, TIOCSCTTY, 0));
        assert(dup2(slave, 0) >= 0 && dup2(slave, 1) >= 0 && dup2(slave, 2) >= 0);
        close(slave);
        setenv("TERM", "xterm-256color", 1);
        setenv("LC_ALL", "C", 1);
        if (color) unsetenv("NO_COLOR"); else setenv("NO_COLOR", "1", 1);
        const char *binary = getenv("TINY_CHESS_TEST_BINARY");
        execl(binary ? binary : "./tiny-chess", "tiny-chess", (char *)NULL);
        _exit(127);
    }
    close(slave);
    collect();
    expect("\033[?1049h"); expect("\033[?1000h\033[?1006h");
    expect("White to move");
}

static void finished(void)
{
    expect("\033[?1000l\033[?1006l"); expect("\033[?25h\033[?1049l");
    expect("Goodbye!");
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    struct termios restored;
    assert(!tcgetattr(master, &restored));
    assert(restored.c_lflag == original.c_lflag && restored.c_iflag == original.c_iflag);
    assert(restored.c_cc[VMIN] == original.c_cc[VMIN] && restored.c_cc[VTIME] == original.c_cc[VTIME]);
    close(master);
}

int main(void)
{
    alarm(30);
    launch(0);
    send_text("\033[<0;18;11M"); expect("{P}"); expect("(.)");
    send_text("\033[<0;18;11m\033[<64;18;11M\033[A"); assert(!*output);
    send_text("\033[<0;18;8M"); expect("Illegal move"); expect("White to move"); expect("{P}");
    send_text("\033[<0;18;9M"); expect("Black to move");
    send_text("c7 c5\r"); expect("White to move");
    send_text("\033[<0;8;18M"); expect("New game."); expect("White to move");
    send_text("e2 e5\1774\r"); expect("Black to move");
    send_text("\033[<0;18;6M"); expect("{p}");
    send_text("\033[<2;18;6M"); expect("Selection cancelled.");
    send_text("\033[<0;18;6M\033"); expect("Selection cancelled.");
    struct winsize size = { .ws_row = 16, .ws_col = 40 };
    assert(!ioctl(master, TIOCSWINSZ, &size)); collect(); expect("Enlarge terminal");
    send_text("\033[<0;27;18M"); expect("Enlarge terminal");
    size.ws_row = 24; size.ws_col = 80;
    assert(!ioctl(master, TIOCSWINSZ, &size)); collect(); expect("Black to move");
    send_text("\033[<0;27;18M"); finished();
    puts("PTY: click/release, scroll/arrows, keyboard/backspace, reset, cancel, resize, quit: PASS");

    launch(1);
    expect("\033[47;30m");
    send_text("\033[<0;18;11M"); expect("\033[43;30m"); expect("\033[42;30m");
    send_text("q\r"); finished();
    puts("PTY: color highlights and keyboard quit restore terminal: PASS");

    launch(0);
    assert(!kill(child, SIGTERM)); collect(); finished();
    puts("PTY: SIGTERM restores terminal and mouse modes: PASS");
    launch(0);
    send_text("\003"); finished();
    puts("PTY: Ctrl-C restores terminal and mouse modes: PASS");
    puts("ALL TERMINAL TESTS PASSED");
}
