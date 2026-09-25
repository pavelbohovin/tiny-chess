#define main chess_main
#include "../chess.c"
#undef main
#include <assert.h>
#include <sys/stat.h>

static char work[] = "/tmp/tiny-chess-invite.XXXXXX", capture[256], helper[256];

static void script(const char *body)
{
    FILE *file = fopen(helper, "w");
    assert(file && fputs(body, file) >= 0 && !fclose(file) && !chmod(helper, 0700));
}

static void content(const char *path, char *buffer, size_t size)
{
    FILE *file = fopen(path, "r");
    assert(file);
    size_t n = fread(buffer, 1, size - 1, file);
    assert(!ferror(file) && feof(file));
    buffer[n] = '\0';
    assert(!fclose(file));
}

static void launch(const char *port, const char *host, int copy, int status, char *output, size_t size)
{
    FILE *input = tmpfile(), *result = tmpfile();
    assert(input && result && fputs("q\n", input) >= 0 && !fflush(input));
    rewind(input);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        assert(dup2(fileno(input), 0) >= 0 && dup2(fileno(result), 1) >= 0 && dup2(fileno(result), 2) >= 0);
        fclose(input); fclose(result);
        const char *binary = getenv("TINY_CHESS_TEST_BINARY");
        char *args[] = { (char *)(binary ? binary : "./tiny-chess"), "--ascii", "--host", (char *)port,
                         copy ? "--ascii" : "--no-clipboard", "--share-host", (char *)host, NULL };
        if (!host) args[5] = NULL;
        execv(args[0], args);
        _exit(127);
    }
    int got;
    assert(waitpid(child, &got, 0) == child && WIFEXITED(got) && WEXITSTATUS(got) == status);
    rewind(result);
    size_t n = fread(output, 1, size - 1, result);
    assert(!ferror(result) && feof(result));
    output[n] = '\0';
    fclose(input); fclose(result);
}

int main(void)
{
    alarm(20);
    assert(mkdtemp(work));
    snprintf(capture, sizeof capture, "%s/clipboard", work);
#ifdef __APPLE__
    snprintf(helper, sizeof helper, "%s/pbcopy", work);
#else
    snprintf(helper, sizeof helper, "%s/wl-copy", work);
    setenv("WAYLAND_DISPLAY", "test", 1);
    unsetenv("DISPLAY");
#endif
    setenv("PATH", work, 1); /* Never touch the user's real clipboard. */
    setenv("TINY_CHESS_TEST_CLIPBOARD", capture, 1);
    script("#!/bin/sh\n/bin/cat > \"$TINY_CHESS_TEST_CLIPBOARD\"\n");
    assert(share_host_valid("192.168.1.20") && share_host_valid("chess.example.org"));
    assert(!share_host_valid("") && !share_host_valid("-flag") && !share_host_valid("a;touch /tmp/oops"));
    assert(!share_host_valid("$(id)") && !share_host_valid("a\nb") && !share_host_valid("::1"));

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = { .sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK) };
    assert(fd >= 0 && !bind(fd, (struct sockaddr *)&address, sizeof address));
    socklen_t length = sizeof address;
    assert(!getsockname(fd, (struct sockaddr *)&address, &length));
    char port[8], output[4096], copied[512], expected[512];
    snprintf(port, sizeof port, "%u", ntohs(address.sin_port));
    close(fd);
    launch(port, "192.168.1.20", 1, 0, output, sizeof output);
    content(capture, copied, sizeof copied);
    snprintf(expected, sizeof expected,
             "sh -c \"$(curl -fsSL https://raw.githubusercontent.com/pavelbohovin/tiny-chess/v" VERSION
             "/install.sh)\" -- --join 192.168.1.20 %s", port);
    assert(!strcmp(copied, expected) && !strchr(copied, '\n'));
    assert(strstr(output, "Invite copied!") && strstr(output, expected) && strstr(output, "Goodbye!"));
    assert(!unlink(capture));
    launch(port, "vpn.example.org", 0, 0, output, sizeof output);
    assert(access(capture, F_OK) && strstr(output, "copy manually") && strstr(output, "--join vpn.example.org"));
    launch(port, "bad;host", 1, 1, output, sizeof output);
    assert(access(capture, F_OK) && strstr(output, "needs an IPv4 address or hostname"));

    char ip[INET_ADDRSTRLEN];
    if (host_address(ip)) {
        launch(port, NULL, 1, 0, output, sizeof output);
        content(capture, copied, sizeof copied);
        snprintf(expected, sizeof expected, "--join %s %s", ip, port);
        assert(strstr(copied, expected) && strstr(output, "Invite copied!"));
        assert(!unlink(capture));
    }
    puts("Invite: startup copy, detected IP/custom address, custom port, opt-out, safe arguments: PASS");

    script("#!/bin/sh\nexit 1\n");
    launch(port, "192.168.1.20", 1, 0, output, sizeof output);
    assert(strstr(output, "copy manually") && !strstr(output, "Invite copied!"));
    script("#!/bin/sh\nwhile :; do :; done\n");
    char *args[] = { helper, NULL };
    assert(!clipboard_run(args, "invite"));
    assert(!unlink(helper));
    assert(!clipboard_copy("invite"));
#ifndef __APPLE__
    /* Wayland unavailable: try X11 clipboard owners, then report failure. */
    setenv("DISPLAY", ":test", 1);
    const char *backends[] = { "xclip", "xsel" };
    for (size_t i = 0; i < sizeof backends / sizeof *backends; ++i) {
        snprintf(helper, sizeof helper, "%s/%s", work, backends[i]);
        script("#!/bin/sh\n/bin/cat > \"$TINY_CHESS_TEST_CLIPBOARD\"\n");
        assert(clipboard_copy("X11 invite"));
        content(capture, copied, sizeof copied);
        assert(!strcmp(copied, "X11 invite"));
        assert(!unlink(capture) && !unlink(helper));
    }
    assert(!clipboard_copy("invite"));
    puts("Clipboard: Wayland to X11 fallback (xclip and xsel): PASS");
#endif
    assert(waitpid(-1, NULL, WNOHANG) == -1 && errno == ECHILD);
    assert(!rmdir(work));
    puts("Clipboard: failed/missing helper and timeout recover without claiming success or leaving children: PASS");
    return 0;
}
