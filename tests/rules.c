#define main game_main
#include "../chess.c"
#undef main
#include <assert.h>

static Game fen(const char *text)
{
    Game g = { .turn = 1, .ep = -1 };
    int rank = 7, file = 0;
    while (*text && *text != ' ') {
        unsigned char ch = (unsigned char)*text++;
        if (ch == '/') { --rank; file = 0; }
        else if (isdigit(ch)) file += ch - '0';
        else {
            const char names[] = "pnbrqk";
            const char *p = strchr(names, tolower(ch));
            assert(p && file < 8 && rank >= 0);
            g.board[rank * 16 + file++] = (signed char)((p - names + 1) * (isupper(ch) ? 1 : -1));
        }
    }
    assert(*text++ == ' ');
    g.turn = *text++ == 'w' ? 1 : -1;
    assert(*text++ == ' ');
    while (*text && *text != ' ') {
        switch (*text++) {
        case 'K': g.rights |= WK; break;
        case 'Q': g.rights |= WQ; break;
        case 'k': g.rights |= BK; break;
        case 'q': g.rights |= BQ; break;
        }
    }
    assert(*text++ == ' ');
    if (*text != '-') g.ep = (text[1] - '1') * 16 + text[0] - 'a';
    return g;
}

static int play(Game *g, const char *text)
{
    int from, to, promotion;
    Game next;
    assert(parse(text, &from, &to, &promotion) == 1);
    if (!move(g, from, to, promotion, &next)) return 0;
    *g = next;
    return 1;
}

static unsigned long perft(const Game *g, int depth)
{
    if (!depth) return 1;
    unsigned long count = 0;
    for (int from = 0; from < 128; ++from) {
        if (!inside(from) || sign(g->board[from]) != g->turn) continue;
        for (int to = 0; to < 128; ++to) {
            if (!inside(to)) continue;
            int promotes = abs(g->board[from]) == PAWN && (to >> 4) == (g->turn == 1 ? 7 : 0);
            for (int p = promotes ? KNIGHT : 0; p <= (promotes ? QUEEN : 0); ++p) {
                Game next;
                if (move(g, from, to, p, &next)) count += perft(&next, depth - 1);
            }
        }
    }
    return count;
}

static void expect_perft(const char *name, Game g, int depth, unsigned long expected)
{
    unsigned long got = perft(&g, depth);
    printf("%s depth %d: %lu (expected %lu)\n", name, depth, got, expected);
    fflush(stdout);
    assert(got == expected);
}

int main(void)
{
    Game g, before, n;
    start(&g);
    before = g;
    assert(!play(&g, "e2 e5"));
    assert(!memcmp(&g, &before, sizeof g));
    assert(!play(&g, "e7 e5"));
    assert(!play(&g, "c1 h6"));
    assert(!play(&g, "a1 a3"));
    assert(!play(&g, "d1 d3"));
    assert(!play(&g, "b1 b3"));
    assert(!play(&g, "e1 g1"));
    assert(!play(&g, "e2 e4 q"));
    assert(play(&g, "e2 e4"));
    assert(!play(&g, "e4 e5"));
    assert(play(&g, "d7 d5"));
    assert(play(&g, "e4 d5"));
    assert(g.board[67] == PAWN && !g.board[52]);
    assert(play(&g, "d8 d5"));
    assert(play(&g, "b1 c3"));
    assert(play(&g, "d5 a5"));
    assert(play(&g, "f1 b5"));
    puts("Movement, blocking, captures, turns: PASS");

    start(&g);
    assert(play(&g, "e2e4") && play(&g, "a7a6") && play(&g, "e4e5") && play(&g, "d7d5"));
    assert(play(&g, "e5d6"));
    assert(g.board[83] == PAWN && !g.board[67]);
    start(&g);
    assert(play(&g, "e2e4") && play(&g, "a7a6") && play(&g, "e4e5") && play(&g, "d7d5"));
    assert(play(&g, "h2h3") && play(&g, "a6a5"));
    assert(!play(&g, "e5d6"));
    g = fen("7k/8/8/r4pPK/8/8/8/8 w - f6");
    assert(!play(&g, "g5f6"));
    g = fen("7k/8/8/8/3pP3/8/8/K7 b - e3");
    assert(play(&g, "d4e3") && g.board[36] == -PAWN && !g.board[52]);
    puts("En passant, expiry, discovered-check rejection: PASS");

    g = fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq -");
    assert(play(&g, "e1g1") && g.board[6] == KING && g.board[5] == ROOK);
    assert(!(g.rights & (WK | WQ)) && !g.board[4] && !g.board[7]);
    assert(play(&g, "e8c8") && g.board[114] == -KING && g.board[115] == -ROOK);
    g = fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq -");
    assert(play(&g, "e1c1") && play(&g, "e8g8"));
    g = fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq -");
    assert(play(&g, "h1h2") && play(&g, "h8h7") && play(&g, "h2h1") && play(&g, "h7h8"));
    assert(!play(&g, "e1g1"));
    g = fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq -");
    assert(play(&g, "e1e2") && play(&g, "e8e7") && play(&g, "e2e1") && play(&g, "e7e8"));
    assert(!play(&g, "e1g1") && !play(&g, "e1c1"));
    g = fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq -");
    assert(play(&g, "h1h8") && !(g.rights & (WK | BK)));
    g = fen("4kr2/8/8/8/8/8/8/4K2R w K -");
    assert(!play(&g, "e1g1"));
    g = fen("4k1r1/8/8/8/8/8/8/4K2R w K -");
    assert(!play(&g, "e1g1"));
    g = fen("k3r3/8/8/8/8/8/8/4K2R w K -");
    assert(!play(&g, "e1g1"));
    g = fen("4k3/8/8/8/8/8/8/RN2K3 w Q -");
    assert(!play(&g, "e1c1"));
    puts("Both castling sides/colors, rights, attacks, path: PASS");

    for (int p = KNIGHT; p <= QUEEN; ++p) {
        g = fen("8/P6k/8/8/8/8/7p/K7 w - -");
        assert(move(&g, 96, 112, p, &n) && n.board[112] == p);
        g.turn = -1;
        assert(move(&g, 23, 7, p, &n) && n.board[7] == -p);
    }
    g = fen("1r6/P6k/8/8/8/8/8/K7 w - -");
    assert(play(&g, "a7b8=n") && g.board[113] == KNIGHT);
    g = fen("8/P6k/8/8/8/8/8/K7 w - -");
    assert(play(&g, "a7a8") && g.board[112] == QUEEN);
    puts("All promotions, both colors, capture, default queen: PASS");

    g = fen("k3r3/8/8/8/8/8/4R3/4K3 w - -");
    assert(!play(&g, "e2f2"));
    assert(play(&g, "e2e8"));
    g = fen("8/8/8/8/8/4k3/8/4K3 w - -");
    assert(!play(&g, "e1e2"));
    start(&g);
    assert(play(&g, "f2f3") && play(&g, "e7e5") && play(&g, "g2g4") && play(&g, "d8h4"));
    assert(in_check(&g, 1) && !has_move(&g));
    start(&g);
    assert(play(&g, "e2e4") && play(&g, "e7e5") && play(&g, "f1c4") && play(&g, "b8c6"));
    assert(play(&g, "d1h5") && play(&g, "g8f6") && play(&g, "h5f7"));
    assert(in_check(&g, -1) && !has_move(&g));
    g = fen("7k/5Q2/6K1/8/8/8/8/8 b - -");
    assert(!in_check(&g, -1) && !has_move(&g));
    g = fen("7k/8/8/8/8/8/7R/K7 w - -");
    assert(!play(&g, "h2h8"));
    puts("Pins, adjacent kings, check, both mates, stalemate: PASS");

    int from, to, p;
    const char *bad[] = { "", "e", "e2", "e2e", "i2e4", "e0e4", "e2i4", "e2e9", "e2e4qq", "e2e4=", "e2e4k", "e2-e4", "e2e4garbage", "restart", "e2e4=Qmore" };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; ++i) assert(!parse(bad[i], &from, &to, &p));
    assert(parse(" E7 E8 = N\n", &from, &to, &p) == 1 && p == KNIGHT);
    assert(parse("q\n", &from, &to, &p) == 'q');
    assert(parse(" R ", &from, &to, &p) == 'r');
    start(&g);
    assert(!move(&g, -1, 0, 0, &n) && !move(&g, 0, 128, 0, &n));
    puts("Input validation: PASS");

    start(&g);
    expect_perft("Initial", g, 1, 20);
    expect_perft("Initial", g, 2, 400);
    expect_perft("Initial", g, 3, 8902);
    expect_perft("Initial", g, 4, 197281);
    g = fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -");
    expect_perft("Kiwipete", g, 1, 48);
    expect_perft("Kiwipete", g, 2, 2039);
    expect_perft("Kiwipete", g, 3, 97862);
    g = fen("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -");
    expect_perft("Rook/pawn endgame", g, 4, 43238);
    puts("ALL TESTS PASSED");
    return 0;
}
