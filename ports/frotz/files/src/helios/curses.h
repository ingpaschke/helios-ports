/*
 * curses.h -- the part of the ncurses API that the Frotz curses interface
 * uses, on top of the Helios curses library (4.3BSD curses, curses.lib).
 *
 * Found before the real header because src/helios is on the -I list; the
 * Helios <curses.h> comes from the Helios source tree (-idirafter, see
 * Makefile.helios), which is where Helios keeps it.
 *
 * What the BSD library lacks is emulated or switched off here:
 *   - attributes: one "standout" mode (reverse video on Helios windows);
 *     A_REVERSE and A_BOLD map to it, underline/italic are dropped
 *   - colour: none (COLOR_SUPPORT is not defined for the Helios build)
 *   - keypad decoding, getch timeouts, nodelay: hcurses.c reads the
 *     console itself (Helios Read with a timeout) and decodes ANSI and
 *     Helios CSI key sequences into KEY_* codes
 *   - scrolling regions: setscrreg/scrl; a region that reaches the
 *     bottom row scrolls the terminal with a line feed (cheap on the I/O
 *     Servers), the rows above it are repainted; others via
 *     deleteln/insertln
 *   - pads, copywin, mouse, cursor visibility: stubs
 */
#ifndef HELIOS_FROTZ_CURSES_H
#define HELIOS_FROTZ_CURSES_H

/* helios.h first: it typedefs bool (as word), which must not meet the
 * BSD `#define bool char' below. */
#include <helios.h>
#ifndef _BSD
#define _BSD
#define HC_UNDEF_BSD
#endif
#include_next <curses.h>
#ifdef HC_UNDEF_BSD
#undef _BSD
#undef HC_UNDEF_BSD
#endif

/* BSD curses has `#define bool char'; the library's own structures are
 * declared by now.  From here on bool is the helios.h typedef (word, 32
 * bits), the same size as Frotz's `typedef int bool' in the common code. */
#undef bool

/* BSD curses has ERR 0 and OK 1; Frotz expects the ncurses values (ERR
 * must differ from a NUL key).  Only the shim functions below and the
 * stubs return these; Frotz does not test BSD library results. */
#undef ERR
#undef OK
#define ERR (-1)
#define OK 0

typedef int chtype;

#define A_NORMAL	0
#define A_CHARTEXT	0x000000ff
#define A_COLOR		0x0000ff00
#define A_STANDOUT	0x00010000
#define A_UNDERLINE	0x00020000
#define A_REVERSE	0x00040000
#define A_BLINK		0x00080000
#define A_DIM		0x00100000
#define A_BOLD		0x00200000
#define A_ITALIC	0x00400000
#define COLOR_PAIR(n)	(((n) & 0xff) << 8)
#define PAIR_NUMBER(a)	(((a) & A_COLOR) >> 8)

#define COLOR_BLACK	0
#define COLOR_RED	1
#define COLOR_GREEN	2
#define COLOR_YELLOW	3
#define COLOR_BLUE	4
#define COLOR_MAGENTA	5
#define COLOR_CYAN	6
#define COLOR_WHITE	7

/* Key codes as in ncurses. */
#define KEY_DOWN	0402
#define KEY_UP		0403
#define KEY_LEFT	0404
#define KEY_RIGHT	0405
#define KEY_HOME	0406
#define KEY_BACKSPACE	0407
#define KEY_F0		0410
#define KEY_F(n)	(KEY_F0 + (n))
#define KEY_DC		0512
#define KEY_IC		0513
#define KEY_EOL		0517
#define KEY_NPAGE	0522
#define KEY_PPAGE	0523
#define KEY_ENTER	0527
#define KEY_END		0550


WINDOW *hc_initscr(void);
void	hc_endwin(void);
int	hc_getch(void);
void	hc_timeout(int ms);
int	hc_nodelay(int on);
int	hc_attrset(int attrs);
int	hc_setscrreg(int top, int bottom);
int	hc_scrl(int n);
int	hc_beep(void);
int	hc_mvaddnstr(int y, int x, const char *s, int n);

#undef getch
#define initscr()		hc_initscr()
#define endwin()		hc_endwin()
#define getch()			hc_getch()
#define timeout(ms)		hc_timeout(ms)
#define nodelay(w, on)		hc_nodelay(on)
#define attrset(a)		hc_attrset(a)
#define setscrreg(t, b)		hc_setscrreg(t, b)
#define scrl(n)			hc_scrl(n)
#define beep()			hc_beep()
#define mvaddnstr(y, x, s, n)	hc_mvaddnstr(y, x, s, n)
#define keypad(w, on)		0
#define intrflush(w, on)	0
#define curs_set(v)		1
#define bkgdset(ch)		((void)0)
#define has_colors()		0
#define termattrs()		(A_STANDOUT | A_REVERSE | A_BOLD)
#define newpad(l, c)		((WINDOW *)0)
#define copywin(s, d, a, b, c, e, f, g, h)	ERR
#define getmaxy(w)		((w)->_maxy)
#define getmaxx(w)		((w)->_maxx)
#define getmaxyx(w, y, x)	((y) = (w)->_maxy, (x) = (w)->_maxx)

#endif /* HELIOS_FROTZ_CURSES_H */
