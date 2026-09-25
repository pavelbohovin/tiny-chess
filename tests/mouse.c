#define main game_main
#include "../chess.c"
#undef main
#include <assert.h>

static Event at(int square)
{
    return (Event){ .key = MOUSE, .x = BOARD_X + (square & 7) * 3 + 1,
                    .y = BOARD_Y + 7 - (square >> 4) };
}

static UI fresh(void)
{
    return (UI){ .selected = -1, .pending = -1, .fits = 1, .message = "" };
}

int main(void)
{
    Game g, original;
    UI ui = fresh();
    start(&g);
    original = g;
    Event e = mouse_event("[<0;18;11M");
    assert(e.key == MOUSE && e.x == 18 && e.y == 11 && e.button == 0);
    assert(mouse_event("[<0;18;11m").key == IGNORE);
    assert(mouse_event("[<64;18;11M").key == IGNORE);
    assert(mouse_event("[<32;18;11M").key == IGNORE);
    assert(mouse_event("[<1;18;11M").key == IGNORE);
    assert(mouse_event("[<0;0;11M").key == IGNORE);
    assert(mouse_event("[<0;18;0M").key == IGNORE);
    assert(mouse_event("[<0;9999999999999999;11M").key == IGNORE);
    assert(mouse_event("[<0;18;11Mjunk").key == IGNORE);
    assert(mouse_event("[<0;18;M").key == IGNORE);
    assert(mouse_event("[A").key == IGNORE);
    assert(mouse_event("[<16;18;11M").key == MOUSE);
    assert(mouse_event("[<2;18;11M").button == 2);
    puts("SGR clicks, modifiers, releases, scroll, malformed reports: PASS");

    click(&g, &ui, at(100)); /* Cannot select black first. */
    assert(ui.selected == -1 && !memcmp(&g, &original, sizeof g));
    click(&g, &ui, e);
    assert(ui.selected == 20);
    click(&g, &ui, at(68)); /* Illegal e2-e5 keeps selection. */
    assert(ui.selected == 20 && !memcmp(&g, &original, sizeof g));
    click(&g, &ui, at(52));
    assert(g.board[52] == PAWN && !g.board[20] && g.turn == -1 && ui.selected == -1);
    click(&g, &ui, at(99));
    click(&g, &ui, at(67));
    click(&g, &ui, at(52));
    click(&g, &ui, at(67));
    assert(g.board[67] == PAWN && !g.board[52] && g.turn == -1);
    start(&g); ui = fresh();
    click(&g, &ui, at(20)); click(&g, &ui, at(19));
    assert(ui.selected == 19);
    click(&g, &ui, at(19)); assert(ui.selected == -1);
    click(&g, &ui, at(20));
    click(&g, &ui, (Event){ .button = 2 }); assert(ui.selected == -1);
    const Event outside[] = { { .x = 4, .y = 5 }, { .x = 29, .y = 5 },
        { .x = 5, .y = 4 }, { .x = 5, .y = 13 }, { .x = 99999, .y = 99999 } };
    for (size_t i = 0; i < sizeof outside / sizeof *outside; ++i) {
        click(&g, &ui, outside[i]);
        assert(ui.selected == -1 && !memcmp(&g, &original, sizeof g));
    }
    for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
        Game board = { .turn = 1, .ep = -1 };
        int square = y * 16 + x;
        board.board[square] = ROOK;
        for (int offset = -1; offset <= 1; ++offset) {
            UI view = fresh(); Event cell = at(square); cell.x += offset;
            click(&board, &view, cell);
            assert(view.selected == square);
        }
    }
    puts("All 64 cells/edges, selection, turn, legal/illegal moves, captures: PASS");

    const int promotions[] = { QUEEN, ROOK, BISHOP, KNIGHT };
    for (int side = -1; side <= 1; side += 2) for (int i = 0; i < 4; ++i) {
        g = (Game){ .turn = side, .ep = -1 };
        g.board[4] = KING; g.board[116] = -KING;
        int from = side == 1 ? 96 : 16, to = side == 1 ? 112 : 0;
        g.board[from] = (signed char)(side * PAWN);
        ui = fresh();
        click(&g, &ui, at(from)); click(&g, &ui, at(to));
        assert(ui.pending == to && g.board[from] == side * PAWN && g.turn == side);
        click(&g, &ui, (Event){ .x = 13 + 4 * i, .y = MENU_Y });
        assert(g.board[to] == side * promotions[i] && g.turn == -side && ui.pending == -1);
    }
    puts("Click promotion choices, both colors: PASS");

    g = (Game){ .turn = 1, .rights = WK | WQ, .ep = -1 };
    g.board[4] = KING; g.board[116] = -KING; g.board[0] = g.board[7] = ROOK;
    ui = fresh(); click(&g, &ui, at(4)); click(&g, &ui, at(6));
    assert(g.board[6] == KING && g.board[5] == ROOK);
    ui.ended = 1; click(&g, &ui, at(116)); assert(ui.selected == -1);
    assert(click(&g, &ui, (Event){ .x = 5, .y = MENU_Y }) == 'r');
    assert(click(&g, &ui, (Event){ .x = 18, .y = MENU_Y }) == 'a');
    assert(click(&g, &ui, (Event){ .x = 27, .y = MENU_Y }) == 'q');
    ui.fits = 0;
    assert(!click(&g, &ui, at(116)) && !click(&g, &ui, (Event){ .x = 27, .y = MENU_Y }));
    puts("Click castling, game-over guard, menu buttons, resize guard: PASS");
    puts("ALL MOUSE TESTS PASSED");
}
