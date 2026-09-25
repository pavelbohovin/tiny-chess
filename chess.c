#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <langinfo.h>
#include <locale.h>
#include <netdb.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#define VERSION "1.2.0"

/* Use macOS's socket option even when newer SDKs define the Linux send flag. */
#ifdef SO_NOSIGPIPE
#define SEND_FLAGS 0
#else
#define SEND_FLAGS MSG_NOSIGNAL
#endif

enum { PAWN = 1, KNIGHT, BISHOP, ROOK, QUEEN, KING };
enum { WK = 1, WQ = 2, BK = 4, BQ = 8 };

/* 0x88 board: a1 = 0, h1 = 7, a8 = 112. Bit 0x88 marks an edge. */
typedef struct {
    signed char board[128];
    int turn, rights, ep;
} Game;

static const int rays[] = { 1, -1, 16, -16, 15, -15, 17, -17 };
static const int jumps[] = { 14, -14, 18, -18, 31, -31, 33, -33 };
static const char *pieces[2][7] = {
    { " ", "♙", "♘", "♗", "♖", "♕", "♔" },
    { " ", "♟", "♞", "♝", "♜", "♛", "♚" }
};

static int inside(int square)
{
    return square >= 0 && square < 128 && !(square & 0x88);
}

static int sign(int n)
{
    return (n > 0) - (n < 0);
}

static void start(Game *g)
{
    static const int back[] = { ROOK, KNIGHT, BISHOP, QUEEN,
                                KING, BISHOP, KNIGHT, ROOK };
    memset(g, 0, sizeof *g);
    g->turn = 1;
    g->rights = WK | WQ | BK | BQ;
    g->ep = -1;
    for (int x = 0; x < 8; ++x) {
        g->board[x] = (signed char)back[x];
        g->board[16 + x] = PAWN;
        g->board[96 + x] = -PAWN;
        g->board[112 + x] = (signed char)-back[x];
    }
}

static int attacked(const Game *g, int square, int by)
{
    for (int dx = -1; dx <= 1; dx += 2) {
        int from = square - by * 16 + dx;
        if (inside(from) && g->board[from] == by * PAWN)
            return 1;
    }
    for (int i = 0; i < 8; ++i) {
        int from = square + jumps[i];
        if (inside(from) && g->board[from] == by * KNIGHT)
            return 1;
        for (int at = square + rays[i], distance = 1; inside(at);
             at += rays[i], ++distance) {
            int p = g->board[at];
            if (!p)
                continue;
            if (sign(p) == by && (abs(p) == QUEEN ||
                abs(p) == (i < 4 ? ROOK : BISHOP) ||
                (distance == 1 && abs(p) == KING)))
                return 1;
            break;
        }
    }
    return 0;
}

static int in_check(const Game *g, int side)
{
    for (int at = 0; at < 128; ++at)
        if (inside(at) && g->board[at] == side * KING)
            return attacked(g, at, -side);
    return 1;
}

static int corner_right(int square)
{
    switch (square) {
    case 0: return WQ;
    case 7: return WK;
    case 112: return BQ;
    case 119: return BK;
    default: return 0;
    }
}

/* On success, return the complete next position; never change the input. */
static int move(const Game *g, int from, int to, int promotion, Game *next)
{
    if (!inside(from) || !inside(to) || from == to)
        return 0;
    int p = g->board[from], target = g->board[to], side = g->turn;
    int type = abs(p), dx = (to & 7) - (from & 7);
    int dy = (to >> 4) - (from >> 4), castle = 0, ep_capture = 0;
    int promotes = type == PAWN && (to >> 4) == (side == 1 ? 7 : 0);
    if (sign(p) != side || sign(target) == side || abs(target) == KING)
        return 0;
    if (promotion && (!promotes || promotion < KNIGHT || promotion > QUEEN))
        return 0;

    switch (type) {
    case PAWN:
        if (!dx && !target) {
            if (dy != side && !(dy == 2 * side &&
                (from >> 4) == (side == 1 ? 1 : 6) &&
                !g->board[from + side * 16]))
                return 0;
        } else if (abs(dx) == 1 && dy == side) {
            if (!target) {
                if (to != g->ep || g->board[to - side * 16] != -side * PAWN)
                    return 0;
                ep_capture = 1;
            }
        } else {
            return 0;
        }
        break;
    case KNIGHT:
        if (!((abs(dx) == 1 && abs(dy) == 2) ||
              (abs(dx) == 2 && abs(dy) == 1)))
            return 0;
        break;
    case BISHOP:
    case ROOK:
    case QUEEN: {
        int diagonal = abs(dx) == abs(dy), straight = !dx || !dy;
        if ((type == BISHOP && !diagonal) || (type == ROOK && !straight) ||
            (type == QUEEN && !diagonal && !straight))
            return 0;
        int step = sign(dx) + 16 * sign(dy);
        for (int at = from + step; at != to; at += step)
            if (g->board[at])
                return 0;
        break;
    }
    case KING:
        if (abs(dx) > 1 || abs(dy) > 1) {
            int home = side == 1 ? 4 : 116, direction = sign(dx);
            int right = side == 1 ? (dx > 0 ? WK : WQ) : (dx > 0 ? BK : BQ);
            int rook = home + (dx > 0 ? 3 : -4);
            if (from != home || dy || abs(dx) != 2 || !(g->rights & right) ||
                g->board[rook] != side * ROOK || in_check(g, side))
                return 0;
            for (int at = home + direction; at != rook; at += direction)
                if (g->board[at])
                    return 0;
            Game transit = *g;
            transit.board[from] = 0;
            transit.board[from + direction] = (signed char)p;
            if (in_check(&transit, side))
                return 0;
            castle = direction;
        }
        break;
    default:
        return 0;
    }

    Game n = *g;
    n.board[from] = 0;
    n.board[to] = (signed char)(promotes ? side * (promotion ? promotion : QUEEN) : p);
    if (ep_capture)
        n.board[to - side * 16] = 0;
    if (castle) {
        n.board[from + (castle > 0 ? 3 : -4)] = 0;
        n.board[from + castle] = (signed char)(side * ROOK);
    }
    n.rights &= ~(corner_right(from) | corner_right(to));
    if (type == KING)
        n.rights &= ~(side == 1 ? WK | WQ : BK | BQ);
    n.ep = type == PAWN && abs(dy) == 2 ? from + side * 16 : -1;
    n.turn = -side;
    if (in_check(&n, side))
        return 0;
    *next = n;
    return 1;
}

static int has_move(const Game *g)
{
    Game next;
    for (int from = 0; from < 128; ++from)
        if (inside(from) && sign(g->board[from]) == g->turn)
            for (int to = 0; to < 128; ++to)
                if (inside(to) && move(g, from, to, 0, &next))
                    return 1;
    return 0;
}

/* Ignore spaces; accept e2 e4, e2e4, e7 e8 n, and e7e8=N. */
static int parse(const char *line, int *from, int *to, int *promotion)
{
    char text[8];
    size_t count = 0;
    for (; *line; ++line) {
        unsigned char ch = (unsigned char)*line;
        if (isspace(ch))
            continue;
        if (count == sizeof text - 1)
            return 0;
        text[count++] = (char)tolower(ch);
    }
    text[count] = '\0';
    if (count == 1 && strchr("qra", text[0]))
        return text[0];
    if (count != 4 && count != 5 && !(count == 6 && text[4] == '='))
        return 0;
    for (int i = 0; i < 4; i += 2)
        if (text[i] < 'a' || text[i] > 'h' || text[i + 1] < '1' || text[i + 1] > '8')
            return 0;
    *from = (text[1] - '1') * 16 + text[0] - 'a';
    *to = (text[3] - '1') * 16 + text[2] - 'a';
    *promotion = 0;
    if (count > 4) {
        static const char choices[] = "nbrq";
        const char *choice = strchr(choices, text[count - 1]);
        if (!choice)
            return 0;
        *promotion = KNIGHT + (int)(choice - choices);
    }
    return 1;
}

/* Mouse coordinates are 1-based terminal cells; keep these aligned with draw. */
enum { BOARD_X = 5, BOARD_Y = 5, MENU_Y = 18, MOUSE = 256, IGNORE };
typedef struct {
    int unicode, color, ended, selected, pending, fits;
    char line[128];
    const char *message;
} UI;
typedef struct { int key, button, x, y; } Event;

/* A fixed 12-byte, versioned protocol: header, sequence, move, reserved byte. */
enum { PACKET_SIZE = 12 };
static struct {
    int side, fd, listener, connecting, ready, waiting, lost;
    uint32_t sequence;
    unsigned char input[PACKET_SIZE], output[PACKET_SIZE * 32];
    size_t received, queued;
    time_t deadline;
    char status[128];
} net = { .fd = -1, .listener = -1 };

static struct termios saved_terminal;
static int interactive;
static volatile sig_atomic_t stopped, resized, suspended;

static void on_signal(int sig)
{
    if (sig == SIGWINCH || sig == SIGCONT) resized = 1;
    else if (sig == SIGTSTP) suspended = 1;
    else stopped = 1;
}

static void terminal_end(void)
{
    if (!interactive) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_terminal);
    fputs("\033[?1000l\033[?1006l\033[0m\033[?25h\033[?1049l", stdout);
    fflush(stdout);
    interactive = 0;
}

static int terminal_begin(void)
{
    if (tcgetattr(STDIN_FILENO, &saved_terminal)) return 0;
    struct termios mode = saved_terminal;
    mode.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    mode.c_iflag &= (tcflag_t)~(IXON | ICRNL);
    mode.c_cc[VMIN] = 1;
    mode.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &mode)) return 0;
    interactive = 1;
    fputs("\033[?1049h\033[?25l\033[?1000h\033[?1006h", stdout);
    fflush(stdout);
    return 1;
}

static int read_byte(int timeout)
{
    struct pollfd fd = { .fd = STDIN_FILENO, .events = POLLIN };
    if (poll(&fd, 1, timeout) <= 0) return IGNORE;
    unsigned char ch;
    return read(STDIN_FILENO, &ch, 1) == 1 ? ch : -1;
}

static Event mouse_event(const char *sequence)
{
    Event event = { .key = IGNORE };
    unsigned int button, x, y;
    char end;
    int used = 0;
    /* Bounded fields also reject oversized/malformed reports without overflow. */
    if (sscanf(sequence, "[<%5u;%5u;%5u%c%n", &button, &x, &y, &end, &used) == 4 &&
        !sequence[used] && end == 'M' && button < 32 &&
        ((button & 3) == 0 || (button & 3) == 2) && x && y) {
        event.key = MOUSE;
        event.button = (int)(button & 3);
        event.x = (int)x;
        event.y = (int)y;
    }
    return event;
}

static Event read_event(void)
{
    Event event = { .key = read_byte(100) };
    if (event.key != 27) return event;
    int ch = read_byte(50);
    if (ch == IGNORE) return event; /* A lone Escape cancels selection. */
    event.key = IGNORE;
    if (ch != '[' && ch != 'O') return event;
    char sequence[64] = { (char)ch };
    for (size_t n = 1; n < sizeof sequence - 1; ++n) {
        ch = read_byte(50);
        if (ch < 0 || ch == IGNORE) return event;
        sequence[n] = (char)ch;
        sequence[n + 1] = '\0';
        if (ch >= 0x40 && ch <= 0x7e)
            return mouse_event(sequence); /* Releases, motion, scroll, arrows: ignore. */
    }
    return event;
}

static time_t now(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec;
}

static void network_close(void)
{
    if (net.fd >= 0) close(net.fd);
    if (net.listener >= 0) close(net.listener);
    net.fd = net.listener = -1;
}

static int share_host_valid(const char *host)
{
    return *host && *host != '-' && strlen(host) <= 253 &&
           strspn(host, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-_") == strlen(host);
}

static int host_address(char address[INET_ADDRSTRLEN])
{
    /* UDP connect only asks the routing table; no packet is sent. */
    struct sockaddr_in route = { .sin_family = AF_INET, .sin_port = htons(53) };
    inet_pton(AF_INET, "1.1.1.1", &route.sin_addr);
    int fd = socket(AF_INET, SOCK_DGRAM, 0), found = 0;
    socklen_t length = sizeof route;
    if (fd >= 0) {
        if (!connect(fd, (struct sockaddr *)&route, sizeof route) &&
            !getsockname(fd, (struct sockaddr *)&route, &length) &&
            route.sin_addr.s_addr && (ntohl(route.sin_addr.s_addr) >> 24) != 127)
            found = inet_ntop(AF_INET, &route.sin_addr, address, INET_ADDRSTRLEN) != NULL;
        close(fd);
    }
    if (found) return 1;
    /* A LAN can still work without a default route or internet access. */
    struct ifaddrs *interfaces;
    if (getifaddrs(&interfaces)) return 0;
    for (struct ifaddrs *p = interfaces; p; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET ||
            !(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK)) continue;
        struct in_addr ip = ((struct sockaddr_in *)p->ifa_addr)->sin_addr;
        if (ip.s_addr && (ntohl(ip.s_addr) >> 24) != 127 &&
            inet_ntop(AF_INET, &ip, address, INET_ADDRSTRLEN)) { found = 1; break; }
    }
    freeifaddrs(interfaces);
    return found;
}

static int clipboard_run(char *const args[], const char *text)
{
    int input[2], status;
    size_t length = strlen(text);
    /* Less than POSIX's minimum pipe capacity: fill before forking, no SIGPIPE. */
    if (length >= 512 || pipe(input)) return 0;
    ssize_t written;
    do { written = write(input[1], text, length); } while (written < 0 && errno == EINTR);
    close(input[1]);
    if (written != (ssize_t)length) { close(input[0]); return 0; }
    pid_t child = fork();
    if (!child) {
        network_close(); /* Clipboard owners must not keep the listening port open. */
        if (dup2(input[0], STDIN_FILENO) < 0) _exit(127);
        if (input[0] != STDIN_FILENO) close(input[0]);
        int sink = open("/dev/null", O_WRONLY);
        if (sink < 0 || dup2(sink, STDOUT_FILENO) < 0 || dup2(sink, STDERR_FILENO) < 0)
            _exit(127);
        if (sink > STDERR_FILENO) close(sink);
        execvp(args[0], args);
        _exit(127);
    }
    close(input[0]);
    if (child < 0) return 0;
    for (int tries = 0; tries < 100; ++tries) {
        pid_t result = waitpid(child, &status, WNOHANG);
        if (result == child) return WIFEXITED(status) && !WEXITSTATUS(status);
        if (result < 0 && errno != EINTR) return 0;
        poll(NULL, 0, 20);
    }
    kill(child, SIGKILL);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    return 0;
}

static int clipboard_copy(const char *text)
{
#ifdef __APPLE__
    char *args[] = { "pbcopy", NULL };
    return clipboard_run(args, text);
#else
    char *wayland[] = { "wl-copy", "--type", "text/plain;charset=utf-8", NULL };
    char *xclip[] = { "xclip", "-selection", "clipboard", NULL };
    char *xsel[] = { "xsel", "--clipboard", "--input", NULL };
    if (getenv("WAYLAND_DISPLAY") && clipboard_run(wayland, text)) return 1;
    return getenv("DISPLAY") && (clipboard_run(xclip, text) || clipboard_run(xsel, text));
#endif
}

static void share_invite(const char *host, const char *port, int copy)
{
    char address[INET_ADDRSTRLEN], command[512];
    if (!host) {
        if (!host_address(address)) {
            snprintf(net.status, sizeof net.status, "No network IP found. Restart with --share-host HOST.");
            fprintf(stderr, "%s\n", net.status);
            return;
        }
        host = address;
    }
    snprintf(command, sizeof command,
             "sh -c \"$(curl -fsSL https://raw.githubusercontent.com/pavelbohovin/tiny-chess/v" VERSION
             "/install.sh)\" -- --join %s %s", host, port);
    int copied = copy && clipboard_copy(command);
    /* Print before entering the alternate screen so the invite is in scrollback. */
    printf("Friend command%s:\n%s\n", copied ? " copied to clipboard" : " (copy manually)", command);
    fflush(stdout);
    snprintf(net.status, sizeof net.status, "%s %.32s:%s",
             copied ? "Invite copied! Waiting at" : "Invite in scrollback. Waiting at", host, port);
}

static int nonblocking(int fd)
{
#ifdef SO_NOSIGPIPE
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof yes)) return 0;
#endif
    int flags = fcntl(fd, F_GETFL);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static void disconnected(UI *ui, const char *reason)
{
    network_close();
    net.ready = net.waiting = 0;
    net.lost = 1;
    ui->selected = ui->pending = -1;
    ui->line[0] = '\0';
    snprintf(net.status, sizeof net.status, "%s Quit and host/join again.", reason);
    ui->message = net.status;
}

static int network_open(int side, const char *host, const char *port)
{
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM,
                             .ai_flags = AI_NUMERICSERV }, *addresses;
    if (side == 1) hints.ai_flags |= AI_PASSIVE;
    int error = getaddrinfo(host, port, &hints, &addresses);
    if (error) {
        fprintf(stderr, "Address: %s\n", gai_strerror(error));
        return 0;
    }
    net.side = side;
    int fd = -1, saved_error = 0;
    for (struct addrinfo *a = addresses; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) { saved_error = errno; continue; }
        if (!nonblocking(fd)) goto failed;
        if (side == 1) {
            int yes = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
            if (!bind(fd, a->ai_addr, a->ai_addrlen) && !listen(fd, 1)) break;
        } else {
            if (!connect(fd, a->ai_addr, a->ai_addrlen) || errno == EINPROGRESS) break;
        }
failed:
        saved_error = errno;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(addresses);
    if (fd < 0) {
        errno = saved_error;
        perror(side == 1 ? "Cannot host" : "Cannot connect");
        return 0;
    }
    if (side == 1) {
        net.listener = fd;
        snprintf(net.status, sizeof net.status, "Waiting. Friend: ./tiny-chess --join YOUR_IP %s", port);
    } else {
        net.fd = fd;
        net.connecting = 1;
        net.deadline = now() + 15;
        snprintf(net.status, sizeof net.status, "Connecting to %.48s:%s...", host, port);
    }
    return 1;
}

static int send_packet(UI *ui, int type, int from, int to, int promotion)
{
    if (net.fd < 0 || net.queued + PACKET_SIZE > sizeof net.output) {
        disconnected(ui, "Connection unavailable.");
        return 0;
    }
    unsigned char *p = net.output + net.queued;
    p[0] = 'T'; p[1] = 'C'; p[2] = 1; p[3] = (unsigned char)type;
    for (int i = 0; i < 4; ++i)
        p[4 + i] = (unsigned char)(net.sequence >> (24 - 8 * i));
    p[8] = (unsigned char)from; p[9] = (unsigned char)to;
    p[10] = (unsigned char)promotion; p[11] = 0;
    net.queued += PACKET_SIZE;
    return 1;
}

static int flush_packets(UI *ui)
{
    while (net.queued && net.fd >= 0) {
        ssize_t sent = send(net.fd, net.output, net.queued, SEND_FLAGS);
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return 1;
        if (sent <= 0) { disconnected(ui, "Friend disconnected."); return 0; }
        net.queued -= (size_t)sent;
        memmove(net.output, net.output + sent, net.queued);
    }
    return 1;
}

static int can_play(const Game *g, UI *ui)
{
    if (!net.side) return 1;
    if (!net.ready) {
        ui->message = net.status;
        return 0;
    }
    if (g->turn != net.side || net.waiting) {
        ui->message = net.waiting ? "Move sent. Waiting for host." : "Waiting for your friend's move.";
        return 0;
    }
    return 1;
}

static int apply_move(Game *g, UI *ui, int from, int to, int promotion)
{
    Game next;
    if (!move(g, from, to, promotion, &next)) {
        ui->message = "Illegal move. Try again.";
        return 0;
    }
    *g = next;
    ui->ended = !has_move(g);
    ui->selected = ui->pending = -1;
    ui->line[0] = '\0';
    ui->message = "";
    return 1;
}

static void restart(Game *g, UI *ui)
{
    start(g);
    ui->ended = 0;
    ui->selected = ui->pending = -1;
    ui->line[0] = '\0';
    ui->message = "New game.";
    net.waiting = 0;
}

static int finish_move(Game *g, UI *ui, int from, int to, int promotion)
{
    if (!can_play(g, ui)) return 0;
    Game next;
    if (!move(g, from, to, promotion, &next)) {
        ui->message = "Illegal move. Try again.";
        return 0;
    }
    if (net.side) {
        if (!send_packet(ui, 'M', from, to, promotion)) return 0;
        if (net.side == -1) {
            net.waiting = 1;
            ui->selected = ui->pending = -1;
            ui->line[0] = '\0';
            ui->message = "Move sent. Waiting for host.";
            return 1;
        }
        ++net.sequence;
    }
    return apply_move(g, ui, from, to, promotion);
}

static int receive_packet(Game *g, UI *ui)
{
    const unsigned char *p = net.input;
    uint32_t sequence = 0;
    for (int i = 0; i < 4; ++i) sequence = (sequence << 8) | p[4 + i];
    int type = p[3], from = p[8], to = p[9], promotion = p[10];
    if (p[0] != 'T' || p[1] != 'C' || p[2] != 1 || p[11] ||
        (type != 'M' && (from || to || promotion))) goto invalid;
    if (!net.ready) {
        if (type != 'H' || sequence) goto invalid;
        net.ready = 1;
        snprintf(net.status, sizeof net.status, "Connected. You play %s.", net.side == 1 ? "White" : "Black");
        ui->message = net.status;
        return 1;
    }
    if (type == 'M') {
        Game next;
        int valid = sequence == net.sequence && !ui->ended &&
                    (net.side != 1 || g->turn == -1) && move(g, from, to, promotion, &next);
        if (!valid) {
            if (net.side == 1) return send_packet(ui, 'E', 0, 0, 0);
            goto invalid;
        }
        if (net.side == 1 && !send_packet(ui, 'M', from, to, promotion)) return 0;
        apply_move(g, ui, from, to, promotion);
        ++net.sequence;
        net.waiting = 0;
        return 1;
    }
    if (type == 'R' && net.side == -1 && sequence == net.sequence) {
        restart(g, ui);
        ++net.sequence;
        ui->message = "Host started a new game.";
        return 1;
    }
    if (type == 'E' && net.side == -1 && sequence == net.sequence) {
        net.waiting = 0;
        ui->message = "Host rejected the move. Try again.";
        return 1;
    }
invalid:
    disconnected(ui, "Incompatible or invalid game data.");
    return 0;
}

/* Nonblocking socket work keeps the board responsive while the friend thinks. */
static int network_update(Game *g, UI *ui)
{
    int dirty = 0;
    if (!net.side || net.lost) return 0;
    if (net.listener >= 0) {
        int fd = accept(net.listener, NULL, NULL);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return 0;
            disconnected(ui, "Could not accept friend.");
            return 1;
        }
        close(net.listener); net.listener = -1;
        net.fd = fd;
        if (!nonblocking(fd)) { disconnected(ui, "Socket error."); return 1; }
        net.deadline = now() + 15;
        snprintf(net.status, sizeof net.status, "Friend connected. Starting game...");
        ui->message = net.status;
        if (!send_packet(ui, 'H', 0, 0, 0)) return 1;
        dirty = 1;
    }
    if (net.connecting) {
        struct pollfd fd = { .fd = net.fd, .events = POLLOUT };
        if (poll(&fd, 1, 0) > 0) {
            int error = 0;
            socklen_t size = sizeof error;
            if (getsockopt(net.fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0) error = errno;
            if (error) { disconnected(ui, strerror(error)); return 1; }
            net.connecting = 0;
            if (!send_packet(ui, 'H', 0, 0, 0)) return 1;
            dirty = 1;
        }
    }
    if (!net.ready && now() >= net.deadline) {
        disconnected(ui, "Connection timed out.");
        return 1;
    }
    if (net.connecting) return dirty;
    if (!flush_packets(ui)) return 1;
    for (int count = 0; count < 32; ++count) {
        ssize_t got = recv(net.fd, net.input + net.received, PACKET_SIZE - net.received, 0);
        if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) break;
        if (got <= 0) { disconnected(ui, "Friend disconnected."); return 1; }
        net.received += (size_t)got;
        if (net.received == PACKET_SIZE) {
            net.received = 0;
            dirty = 1;
            if (!receive_packet(g, ui)) return 1;
        }
    }
    if (!flush_packets(ui)) return 1;
    return dirty;
}

static int click(Game *g, UI *ui, Event event)
{
    if (!ui->fits) return 0;
    if (event.button == 2) {
        ui->selected = ui->pending = -1;
        ui->message = "Selection cancelled.";
        return 0;
    }
    if (ui->pending >= 0) {
        if (event.y == MENU_Y && event.x >= 12 && event.x <= 26 &&
            (event.x - 12) % 4 < 3) {
            const int choices[] = { QUEEN, ROOK, BISHOP, KNIGHT };
            finish_move(g, ui, ui->selected, ui->pending, choices[(event.x - 12) / 4]);
        }
        return 0;
    }
    if (event.y == MENU_Y) {
        if (event.x >= 3 && event.x <= 12) return 'r';
        if (event.x >= 15 && event.x <= 21) return 'a';
        if (event.x >= 24 && event.x <= 29) return 'q';
    }
    if (event.x < BOARD_X || event.x >= BOARD_X + 24 ||
        event.y < BOARD_Y || event.y >= BOARD_Y + 8) return 0;
    if (ui->ended) {
        ui->message = "Game over. Click New game to restart.";
        return 0;
    }
    if (!can_play(g, ui)) return 0;
    int square = (7 - (event.y - BOARD_Y)) * 16 + (event.x - BOARD_X) / 3;
    ui->line[0] = '\0';
    if (sign(g->board[square]) == g->turn) {
        ui->selected = ui->selected == square ? -1 : square;
        ui->message = ui->selected < 0 ? "Selection cancelled." : "Click a destination. Dots mark legal moves.";
    } else if (ui->selected >= 0) {
        Game next;
        if (abs(g->board[ui->selected]) == PAWN &&
            (square >> 4) == (g->turn == 1 ? 7 : 0) &&
            move(g, ui->selected, square, 0, &next)) {
            ui->pending = square;
            ui->message = "Choose a promotion piece below, or press q/r/b/n.";
        } else {
            finish_move(g, ui, ui->selected, square, 0);
        }
    } else {
        ui->message = "Click a piece belonging to the side whose turn it is.";
    }
    return 0;
}

static void draw(const Game *g, UI *ui)
{
    if (interactive) {
        struct winsize size;
        fputs("\033[H\033[2J", stdout);
        ui->fits = ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) != 0 ||
                   !size.ws_col || !size.ws_row || (size.ws_col >= 64 && size.ws_row >= 21);
        if (!ui->fits) {
            fputs("Enlarge terminal to 64 columns x 21 rows.\r\nq + Enter: quit.", stdout);
            fflush(stdout);
            return;
        }
    }
    printf("\n  TINY CHESS  |  %s\n\n     a  b  c  d  e  f  g  h\n",
           net.side == 1 ? "network / You: White (host)" :
           net.side == -1 ? "network / You: Black (friend)" : "local two-player");
    for (int y = 7; y >= 0; --y) {
        printf("  %d ", y + 1);
        for (int x = 0; x < 8; ++x) {
            int square = y * 16 + x, p = g->board[square];
            Game next;
            int legal = ui->selected >= 0 && move(g, ui->selected, square, 0, &next);
            int selected = square == ui->selected;
            char ascii[] = { " PNBRQK"[abs(p)], '\0' };
            if (p < 0) ascii[0] = (char)tolower((unsigned char)ascii[0]);
            const char *glyph = legal && !p ? "." : ui->unicode ? pieces[p < 0][abs(p)] : ascii;
            if (interactive && ui->color)
                printf("\033[%d;30m %s \033[0m", selected ? 43 : legal ? 42 : (x + y) % 2 ? 47 : 46, glyph);
            else
                printf("%c%s%c", selected ? '{' : legal ? '(' : '[', glyph, selected ? '}' : legal ? ')' : ']');
        }
        printf(" %d\n", y + 1);
    }
    puts("     a  b  c  d  e  f  g  h\n");
    if (interactive) fputs("\033[1m", stdout);
    if (net.side && !net.ready)
        puts(net.lost ? "  Disconnected" : "  Waiting for connection...");
    else if (ui->ended)
        puts(in_check(g, g->turn) ?
             (g->turn == 1 ? "  Checkmate! Black wins." : "  Checkmate! White wins.") :
             "  Stalemate! Draw.");
    else
        printf("  %s to move%s%s\n", g->turn == 1 ? "White" : "Black",
               in_check(g, g->turn) ? " -- CHECK!" : "",
               !net.side ? "" : g->turn == net.side ? " / Your turn" : " / Friend's turn");
    if (interactive) fputs("\033[0m", stdout);
    puts("  Type: e2 e4 | Promote: e7 e8 q/r/b/n | Enter to submit");
    puts(interactive ? "  Click piece, then destination | Esc/right click: cancel" :
                       "  q: quit | r: restart | a: ASCII toggle");
    puts(ui->pending >= 0 ? "  Promote: [Q] [R] [B] [N] (Enter = Q)" :
                           "  [New game]  [ASCII]  [Quit]   (keys: r, a, q + Enter)");
    printf("  %.60s\n> %s", ui->message, ui->line);
    fflush(stdout);
}

static void usage(void)
{
    puts("Usage: tiny-chess [--ascii] [--host [PORT] | --join HOST [PORT]]\n"
         "Local: ./tiny-chess\nHost (White): ./tiny-chess --host 5555\n"
         "Friend (Black): ./tiny-chess --join HOST_IP 5555\n"
         "Both players need tiny-chess. Use a reachable IPv4 address or hostname.\n"
         "Use the same LAN, a VPN address, or forward the TCP port on your router.\n"
         "Hosting copies an install-and-join command to your clipboard.\n"
         "--share-host HOST: override the detected IP (VPN/public address).\n"
         "--no-clipboard: print the invite without changing your clipboard.\n"
         "--version: print the version.\n"
         "Click a piece, then its destination. Esc/right click cancels.\n"
         "Moves: e2 e4; promotion: e7 e8 q/r/b/n (default: queen).\n"
         "Castle: click king then g/c square, or type e1 g1/c1 (Black: rank 8).\n"
         "q: quit; r: restart (host only online); a: ASCII toggle. Enter submits.");
}

int main(int argc, char **argv)
{
    setlocale(LC_CTYPE, "");
    UI ui = { .unicode = strcmp(nl_langinfo(CODESET), "UTF-8") == 0,
              .selected = -1, .pending = -1, .fits = 1,
              .message = "White: uppercase / hollow. Black: lowercase / filled." };
    for (wchar_t ch = L'♔'; ui.unicode && ch <= L'♟'; ++ch)
        ui.unicode = wcwidth(ch) == 1;
    int side = 0, copy = 1;
    const char *host = NULL, *port = "5555", *share_host = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--ascii")) ui.unicode = 0;
        else if (!strcmp(argv[i], "--version")) { puts("tiny-chess " VERSION); return 0; }
        else if (!strcmp(argv[i], "--no-clipboard")) copy = 0;
        else if (!strcmp(argv[i], "--share-host")) {
            if (++i == argc || !share_host_valid(argv[i])) {
                fputs("--share-host needs an IPv4 address or hostname.\n", stderr);
                return 1;
            }
            share_host = argv[i];
        }
        else if (!side && (!strcmp(argv[i], "--host") || !strcmp(argv[i], "--join"))) {
            side = !strcmp(argv[i], "--host") ? 1 : -1;
            if (side == -1) {
                if (++i == argc || argv[i][0] == '-') { usage(); return 1; }
                host = argv[i];
            }
            if (i + 1 < argc && argv[i + 1][0] != '-') port = argv[++i];
        }
        else {
            usage();
            return strcmp(argv[i], "--help") != 0;
        }
    }
    if (share_host && side != 1) {
        fputs("--share-host is only for --host games.\n", stderr);
        return 1;
    }
    if (!*port || strlen(port) > 5 || strspn(port, "0123456789") != strlen(port) ||
        strtol(port, NULL, 10) < 1 || strtol(port, NULL, 10) > 65535) {
        fputs("Port must be a number from 1 to 65535.\n", stderr);
        return 1;
    }
    struct sigaction action = { .sa_handler = on_signal };
    sigemptyset(&action.sa_mask);
    const int signals[] = { SIGINT, SIGTERM, SIGHUP, SIGWINCH, SIGTSTP, SIGCONT };
    for (size_t i = 0; i < sizeof signals / sizeof *signals; ++i)
        sigaction(signals[i], &action, NULL);
    atexit(terminal_end);
    atexit(network_close);
    if (side) {
        if (!network_open(side, host, port)) return 1;
        if (side == 1) share_invite(share_host, port, copy);
        ui.message = net.status;
    }
    const char *term = getenv("TERM");
    if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) && term && strcmp(term, "dumb"))
        terminal_begin();
    ui.color = !getenv("NO_COLOR");
    Game game;
    start(&game);
    int dirty = 1;
    while (!stopped) {
        dirty |= network_update(&game, &ui);
        if (suspended) {
            int was_interactive = interactive;
            terminal_end();
            struct sigaction normal = { .sa_handler = SIG_DFL };
            sigemptyset(&normal.sa_mask);
            sigaction(SIGTSTP, &normal, NULL);
            raise(SIGTSTP);
            sigaction(SIGTSTP, &action, NULL);
            if (was_interactive) terminal_begin();
            suspended = 0;
            dirty = 1;
        }
        if (dirty || resized) {
            resized = 0;
            draw(&game, &ui);
            dirty = 0;
        }
        int command = 0, from = 0, to = 0, promotion = 0;
        if (interactive || net.side) {
            Event event = read_event();
            if (event.key < 0 || event.key == 4) break;
            if (event.key == IGNORE) continue;
            dirty = 1;
            if (event.key == MOUSE) {
                command = click(&game, &ui, event);
                if (!command) continue;
            } else if (event.key == 27) {
                ui.selected = ui.pending = -1;
                ui.line[0] = '\0';
                ui.message = "Selection cancelled.";
                continue;
            } else if (ui.pending >= 0) {
                const char choices[] = "qrbn";
                int key = event.key == '\r' || event.key == '\n' ? 'q' : tolower(event.key);
                const char *choice = key ? strchr(choices, key) : NULL;
                if (choice) finish_move(&game, &ui, ui.selected, ui.pending, QUEEN - (int)(choice - choices));
                continue;
            } else if (event.key == '\r' || event.key == '\n') {
                command = parse(ui.line, &from, &to, &promotion);
                ui.line[0] = '\0';
            } else {
                size_t n = strlen(ui.line);
                if ((event.key == 127 || event.key == 8) && n) ui.line[n - 1] = '\0';
                else if (event.key == 21) ui.line[0] = '\0';
                else if (event.key >= 32 && event.key < 127 && n < 60) {
                    ui.line[n] = (char)event.key;
                    ui.line[n + 1] = '\0';
                }
                if (!interactive) dirty = 0;
                continue;
            }
        } else {
            if (!fgets(ui.line, sizeof ui.line, stdin)) break;
            dirty = 1;
            if (!strchr(ui.line, '\n') && !feof(stdin)) {
                int ch;
                while ((ch = getchar()) != '\n' && ch != EOF) {}
                ui.line[0] = '\0';
                ui.message = "Input too long. Use e2 e4.";
                continue;
            }
            command = parse(ui.line, &from, &to, &promotion);
            ui.line[0] = '\0';
        }
        if (command == 'q') break;
        if (command == 'r') {
            if (net.side && !net.ready) ui.message = net.status;
            else if (net.side == -1) ui.message = "Only the host can restart an online game.";
            else if (!net.side || send_packet(&ui, 'R', 0, 0, 0)) {
                restart(&game, &ui);
                if (net.side) ++net.sequence;
            }
        } else if (command == 'a') {
            ui.unicode = !ui.unicode;
            ui.message = ui.unicode ? "Unicode pieces." : "ASCII pieces.";
        } else if (command != 1) {
            ui.message = "Use e2 e4, optional promotion q/r/b/n, q, r, or a.";
        } else if (ui.ended) {
            ui.message = "Game over. Press r then Enter for a new game, or q to quit.";
        } else {
            finish_move(&game, &ui, from, to, promotion);
        }
    }
    if (net.fd >= 0 && !net.connecting) flush_packets(&ui);
    network_close();
    terminal_end();
    puts("\nGoodbye!");
    return 0;
}
