#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

typedef struct { int fd; pid_t pid; size_t used; char output[65536]; struct termios original; } Peer;
static Peer host = { .fd = -1 }, friend = { .fd = -1 };
static char port[8];

static long millis(void)
{
    struct timespec t;
    assert(!clock_gettime(CLOCK_MONOTONIC, &t));
    return t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void pump(int ms)
{
    long until = millis() + ms;
    do {
        struct pollfd fds[] = { { .fd = host.fd, .events = POLLIN }, { .fd = friend.fd, .events = POLLIN } };
        poll(fds, 2, 10);
        Peer *peers[] = { &host, &friend };
        for (int i = 0; i < 2; ++i) if (fds[i].revents & POLLIN) {
            Peer *p = peers[i];
            ssize_t got = read(p->fd, p->output + p->used, sizeof p->output - p->used - 1);
            if (got > 0) {
                p->used += (size_t)got;
                assert(p->used < sizeof p->output - 1);
                p->output[p->used] = '\0';
            }
        }
    } while (millis() < until);
}

static void expect(Peer *p, const char *text)
{
    long until = millis() + 3000;
    while (!strstr(p->output, text) && millis() < until) pump(20);
    if (!strstr(p->output, text)) {
        fprintf(stderr, "Expected %s in %s\n%s\n", text, p == &host ? "host" : "friend", p->output);
        assert(0);
    }
}

static void clear(void)
{
    pump(120);
    host.used = friend.used = 0;
    host.output[0] = friend.output[0] = '\0';
}

static void send_text(Peer *p, const char *text)
{
    clear();
    assert(write(p->fd, text, strlen(text)) == (ssize_t)strlen(text));
}

static void launch(Peer *p, int hosting)
{
    p->used = 0; p->output[0] = '\0';
    p->fd = posix_openpt(O_RDWR | O_NOCTTY);
    assert(p->fd >= 0 && !grantpt(p->fd) && !unlockpt(p->fd));
    int slave = open(ptsname(p->fd), O_RDWR | O_NOCTTY);
    assert(slave >= 0 && !tcgetattr(slave, &p->original));
    struct winsize size = { .ws_row = 24, .ws_col = 80 };
    assert(!ioctl(slave, TIOCSWINSZ, &size));
    p->pid = fork(); assert(p->pid >= 0);
    if (!p->pid) {
        if (host.fd >= 0) close(host.fd);
        if (friend.fd >= 0 && friend.fd != host.fd) close(friend.fd);
        assert(setsid() >= 0 && !ioctl(slave, TIOCSCTTY, 0));
        assert(dup2(slave, 0) >= 0 && dup2(slave, 1) >= 0 && dup2(slave, 2) >= 0);
        close(slave);
        setenv("TERM", "xterm-256color", 1);
        setenv("LC_ALL", "C", 1); setenv("NO_COLOR", "1", 1);
        const char *binary = getenv("TINY_CHESS_TEST_BINARY");
        if (!binary) binary = "./tiny-chess";
        if (hosting) execl(binary, "tiny-chess", "--ascii", "--host", port, (char *)NULL);
        else execl(binary, "tiny-chess", "--ascii", "--join", "127.0.0.1", port, (char *)NULL);
        _exit(127);
    }
    close(slave);
}

static void finish(Peer *p, int expected)
{
    int status;
    assert(waitpid(p->pid, &status, 0) == p->pid && WIFEXITED(status) && WEXITSTATUS(status) == expected);
    if (!expected) {
        expect(p, "\033[?1000l\033[?1006l");
        struct termios restored;
        assert(!tcgetattr(p->fd, &restored));
        assert(restored.c_lflag == p->original.c_lflag && restored.c_iflag == p->original.c_iflag);
    }
    close(p->fd); p->fd = -1;
}

static void play(Peer *p, const char *move, const char *turn)
{
    send_text(p, move);
    expect(&host, turn); expect(&friend, turn);
}

int main(void)
{
    alarm(60);
    int probe = socket(AF_INET, SOCK_STREAM, 0);
    if (probe < 0) { perror("socket"); return 2; }
    struct sockaddr_in address = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    assert(!bind(probe, (struct sockaddr *)&address, sizeof address));
    socklen_t len = sizeof address;
    assert(!getsockname(probe, (struct sockaddr *)&address, &len));
    snprintf(port, sizeof port, "%u", ntohs(address.sin_port)); close(probe);
    launch(&host, 1); expect(&host, "Waiting for connection");
    send_text(&host, "e2 e4\r"); expect(&host, "Waiting for connection");
    launch(&friend, 0);
    expect(&host, "Connected. You play White."); expect(&friend, "Connected. You play Black.");
    send_text(&friend, "e7 e5\r"); expect(&friend, "Waiting for your friend's move.");
    play(&host, "e2 e4\r", "Black to move");
    send_text(&host, "e7 e5\r"); expect(&host, "Waiting for your friend's move.");
    play(&friend, "\033[<0;18;6M\033[<0;18;8M", "White to move");
    play(&host, "g1 f3\r", "Black to move");
    play(&friend, "b8 c6\r", "White to move");
    play(&host, "f1 c4\r", "Black to move");
    play(&friend, "g8 f6\r", "White to move");
    play(&host, "e1 g1\r", "Black to move");
    expect(&host, "1 [R][N][B][Q][ ][R][K][ ] 1");
    expect(&friend, "1 [R][N][B][Q][ ][R][K][ ] 1");
    send_text(&friend, "r\r"); expect(&friend, "Only the host can restart");
    play(&host, "r\r", "White to move"); expect(&friend, "Host started a new game.");
    puts("TCP: connect, sides, typed/mouse moves, castling, synchronized restart: PASS"); fflush(stdout);

    play(&host, "f2 f3\r", "Black to move");
    play(&friend, "e7 e5\r", "White to move");
    play(&host, "g2 g4\r", "Black to move");
    play(&friend, "d8 h4\r", "Checkmate! Black wins.");
    send_text(&host, "e2 e4\r"); expect(&host, "Game over.");
    play(&host, "r\r", "White to move");
    play(&host, "e2 e4\r", "Black to move");
    play(&friend, "a7 a6\r", "White to move");
    play(&host, "e4 e5\r", "Black to move");
    play(&friend, "d7 d5\r", "White to move");
    play(&host, "e5 d6\r", "Black to move");
    expect(&host, "6 [p][ ][ ][P][ ][ ][ ][ ] 6");
    expect(&friend, "6 [p][ ][ ][P][ ][ ][ ][ ] 6");
    send_text(&friend, "q\r"); expect(&friend, "Goodbye!"); finish(&friend, 0);
    expect(&host, "Friend disconnected.");
    send_text(&host, "d6 d7\r"); expect(&host, "Disconnected");
    send_text(&host, "q\r"); expect(&host, "Goodbye!"); finish(&host, 0);
    puts("TCP: both boards agree on checkmate and en passant; disconnect restores terminals: PASS"); fflush(stdout);

    launch(&friend, 0); expect(&friend, "Connection refused");
    send_text(&friend, "q\r"); expect(&friend, "Goodbye!"); finish(&friend, 0);
    launch(&host, 1); expect(&host, "Waiting for connection");
    launch(&friend, 1); expect(&friend, "Cannot host"); finish(&friend, 1);
    puts("TCP: refused connection and occupied port errors: PASS"); fflush(stdout);

    int raw = socket(AF_INET, SOCK_STREAM, 0); assert(raw >= 0);
    assert(!connect(raw, (struct sockaddr *)&address, sizeof address));
    unsigned char hello[12] = { 'T', 'C', 1, 'H' }, reply[12];
    pump(150);
    assert(recv(raw, reply, sizeof reply, MSG_WAITALL) == 12 && !memcmp(reply, hello, 12));
    clear();
    assert(send(raw, hello, 4, 0) == 4); pump(150);
    assert(!strstr(host.output, "Connected. You play"));
    for (int i = 4; i < 12; ++i) { assert(send(raw, hello + i, 1, 0) == 1); pump(20); }
    expect(&host, "Connected. You play White.");
    send_text(&host, "e2 e4\r"); expect(&host, "Black to move");
    assert(recv(raw, reply, sizeof reply, MSG_WAITALL) == 12 && reply[3] == 'M');
    unsigned char requests[24] = { 'T', 'C', 1, 'M', 0, 0, 0, 1, 100, 68, 0, 0,
                                  'T', 'C', 1, 'M', 0, 0, 0, 2, 6, 37, 0, 0 };
    clear(); assert(send(raw, requests, 3, 0) == 3); pump(100);
    assert(!strstr(host.output, "White to move"));
    assert(send(raw, requests + 3, sizeof requests - 3, 0) == (ssize_t)sizeof requests - 3);
    expect(&host, "White to move"); pump(150);
    assert(recv(raw, requests, sizeof requests, MSG_WAITALL) == 24);
    assert(requests[3] == 'M' && requests[7] == 1 && requests[15] == 'E' && requests[19] == 2);
    hello[2] = 99; clear(); assert(send(raw, hello, sizeof hello, 0) == 12);
    expect(&host, "Incompatible or invalid game data."); close(raw);
    send_text(&host, "q\r"); expect(&host, "Goodbye!"); finish(&host, 0);
    puts("TCP: fragmented/coalesced packets, illegal peer turn, incompatible version: PASS");
    puts("ALL ONLINE INTEGRATION TESTS PASSED");
}
