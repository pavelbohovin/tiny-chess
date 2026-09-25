#define main game_main
#include "../chess.c"
#undef main
#include <assert.h>

static UI fresh(int side)
{
    network_close();
    memset(&net, 0, sizeof net);
    net.side = side; net.ready = 1; net.listener = -1;
    net.fd = dup(STDOUT_FILENO); /* Only the packet queue is exercised here. */
    assert(net.fd >= 0);
    return (UI){ .selected = -1, .pending = -1, .fits = 1, .message = "" };
}

static int packet(Game *g, UI *ui, int type, uint32_t seq, int from, int to, int promotion)
{
    unsigned char p[PACKET_SIZE] = { 'T', 'C', 1, (unsigned char)type };
    for (int i = 0; i < 4; ++i) p[4 + i] = (unsigned char)(seq >> (24 - 8 * i));
    p[8] = (unsigned char)from; p[9] = (unsigned char)to; p[10] = (unsigned char)promotion;
    memcpy(net.input, p, sizeof p);
    return receive_packet(g, ui);
}

int main(void)
{
    Game g, before;
    start(&g);
    UI ui = fresh(1);
    assert(finish_move(&g, &ui, 20, 52, 0));
    assert(g.turn == -1 && net.sequence == 1 && net.queued == PACKET_SIZE);
    assert(net.output[3] == 'M' && net.output[7] == 0 && net.output[8] == 20 && net.output[9] == 52);
    before = g;
    assert(!finish_move(&g, &ui, 100, 68, 0) && !memcmp(&g, &before, sizeof g));
    assert(packet(&g, &ui, 'M', 1, 100, 68, 0));
    assert(g.turn == 1 && net.sequence == 2 && net.output[PACKET_SIZE + 3] == 'M');
    before = g;
    assert(packet(&g, &ui, 'M', 2, 6, 37, 0)); /* Peer cannot play White. */
    assert(!memcmp(&g, &before, sizeof g) && net.sequence == 2 && net.output[2 * PACKET_SIZE + 3] == 'E');
    assert(packet(&g, &ui, 'M', 0, 6, 37, 0));
    assert(!memcmp(&g, &before, sizeof g) && net.sequence == 2);
    assert(!packet(&g, &ui, 'R', 2, 0, 0, 0) && net.lost);
    puts("Host validates moves, side, sequence; echoes accepted moves; owns restart: PASS");

    start(&g); ui = fresh(-1);
    before = g;
    assert(!finish_move(&g, &ui, 20, 52, 0) && !memcmp(&g, &before, sizeof g));
    assert(packet(&g, &ui, 'M', 0, 20, 52, 0));
    assert(net.sequence == 1 && g.turn == -1);
    before = g;
    assert(finish_move(&g, &ui, 100, 68, 0) && net.waiting);
    assert(!memcmp(&g, &before, sizeof g) && net.output[3] == 'M' && net.output[7] == 1);
    assert(!finish_move(&g, &ui, 100, 68, 0) && net.queued == PACKET_SIZE);
    assert(packet(&g, &ui, 'M', 1, 100, 68, 0));
    assert(!net.waiting && net.sequence == 2 && g.turn == 1);
    assert(packet(&g, &ui, 'R', 2, 0, 0, 0));
    assert(net.sequence == 3 && g.turn == 1 && g.board[20] == PAWN && g.board[100] == -PAWN);
    assert(packet(&g, &ui, 'E', 3, 0, 0, 0));
    assert(!net.waiting && net.sequence == 3);
    assert(!packet(&g, &ui, 'M', 1, 100, 68, 0) && net.lost);
    puts("Client waits for acknowledgement, rejects duplicate/out-of-sync events, resets: PASS");

    for (int side = -1; side <= 1; side += 2) {
        start(&g); ui = fresh(side); net.ready = 0;
        assert(packet(&g, &ui, 'H', 0, 0, 0, 0) && net.ready);
        ui = fresh(side); net.ready = 0;
        assert(!packet(&g, &ui, 'M', 0, 20, 52, 0) && net.lost);
        ui = fresh(side); net.ready = 0;
        packet(&g, &ui, 'H', 0, 0, 0, 0);
        net.input[2] = 2;
        assert(!receive_packet(&g, &ui) && net.lost);
    }
    start(&g); ui = fresh(1);
    assert(finish_move(&g, &ui, 20, 52, 0));
    before = g;
    assert(packet(&g, &ui, 'M', 1, 255, 0, 0));
    assert(packet(&g, &ui, 'M', 1, 100, 36, 0));
    assert(packet(&g, &ui, 'M', 1, 100, 68, 255));
    assert(!memcmp(&g, &before, sizeof g));
    net.input[11] = 1;
    assert(!receive_packet(&g, &ui) && net.lost);
    puts("Handshake/version checks, malformed moves, reserved bytes: PASS");

    for (int promotion = KNIGHT; promotion <= QUEEN; ++promotion) {
        g = (Game){ .turn = 1, .ep = -1 };
        g.board[4] = KING; g.board[116] = -KING; g.board[96] = PAWN;
        ui = fresh(-1);
        assert(packet(&g, &ui, 'M', 0, 96, 112, promotion));
        assert(g.board[112] == promotion && net.sequence == 1);
    }
    start(&g); ui = fresh(1); before = g;
    net.queued = sizeof net.output;
    assert(!finish_move(&g, &ui, 20, 52, 0) && net.lost && !memcmp(&g, &before, sizeof g));
    network_close();
    puts("All network promotions and bounded output queue: PASS");
    ui = fresh(1);
    network_close();
    int sockets[2];
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    net.fd = sockets[0];
    assert(nonblocking(net.fd));
    close(sockets[1]);
    assert(send_packet(&ui, 'H', 0, 0, 0));
    assert(!flush_packets(&ui) && net.lost);
    puts("Sending to a closed socket reports disconnect without SIGPIPE: PASS");
    puts("ALL NETWORK UNIT TESTS PASSED");
}
