/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <helios/unix/api.h>
#include <curses.h>
#include <term.h>

int main(int argc, char **argv)
{
    FILE *log = fopen(argc > 1 ? argv[1] : "ncurses.log", "w");
    WINDOW *pad;
    char *cap;
    int key, last = 0, row, col, i, failed = 0;
    if (!log || !initscr()) return 2;
    raw(); noecho(); keypad(stdscr, TRUE); timeout(250);
    cap = tigetstr("cup");
    if (!cap || cap == (char *)-1 || !tparm(cap, 2, 3)) ++failed;
    if (key_defined("\233A") != KEY_UP ||
        key_defined("\2330~") != KEY_F(1) ||
        key_defined("\23339~") != KEY_F(40)) ++failed;
    pad = newpad(80, 100);
    if (!pad) { endwin(); fclose(log); return 3; }
    for (i = 0; i < 80; ++i)
        mvwprintw(pad, i, 0, "Pad line %d: ncurses on Helios", i);
    fprintf(log, "startup=%s TERM=%s term=%s colors=%d size=%dx%d\n",
            failed ? "FAIL" : "PASS", getenv("TERM"), termname(),
            tigetnum("colors"), COLS, LINES);
    {
        unsigned rows = 0, cols = 0;
        int result = hu_terminal_size(1, &rows, &cols);
        fprintf(log, "native=%d %ux%u LINES=%s COLUMNS=%s\n", result, cols, rows,
                getenv("LINES") ? getenv("LINES") : "unset",
                getenv("COLUMNS") ? getenv("COLUMNS") : "unset");
    }
    fflush(log);
    do {
        getmaxyx(stdscr, row, col);
        erase();
        mvprintw(0, 0, "ncurses %s: %dx%d key=%d; q quits",
                 NCURSES_VERSION, col, row, last);
        refresh();
        if (row > 4 && col > 4)
            prefresh(pad, 0, 0, 2, 1, row > 80 ? 79 : row - 2,
                     col > 100 ? 98 : col - 2);
        key = getch();
        if (key != ERR) {
            fprintf(log, "key=%d size=%dx%d\n", key, COLS, LINES);
            fflush(log);
            last = key;
        }
    } while (key != 'q');
    delwin(pad); endwin();
    fprintf(log, "exit=%s\n", failed ? "FAIL" : "PASS");
    fclose(log);
    return failed != 0;
}
