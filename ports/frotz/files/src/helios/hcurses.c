/*
 * hcurses.c -- ncurses-style helpers for the Frotz curses interface on
 * Helios (see curses.h in this directory for the overview).
 *
 * Input does not go through the BSD wgetch(): Helios curses puts the
 * console into raw input mode in initscr (gettmode), and this file reads
 * the stdin stream with the Helios Read(), which takes a timeout, so
 * getch timeouts and nodelay work without select().
 *
 * Keys: Helios windows send CSI (0x9b) sequences, terminals and xterm
 * send ESC [ / ESC O sequences; both are decoded into KEY_* codes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <helios.h>
#include <syslib.h>
#include <attrib.h>
#include <nonansi.h>
#include <sys/time.h>

#include <curses.h>	/* src/helios/curses.h */

#undef initscr
#undef endwin
#undef getch

extern int _ldtimer(int pri);
extern int _putchar(char c);	/* curses.lib putchar.c */

static int hc_delay = -1;		/* getch wait in ms, -1 = forever */
static int hc_top, hc_bottom = -1;	/* scrolling region */
static unsigned char hc_pend[16];	/* bytes read ahead */
static int hc_npend;
static int hc_tty = -1;
static Attributes hc_saved;
static int hc_have_saved;

WINDOW *hc_initscr(void)
{
	Stream *s = fdstream(0);
	char *e;
	WINDOW *w;

	if (s != NULL && GetAttributes(s, &hc_saved) >= 0)
		hc_have_saved = 1;
	if (getenv("TERM") == NULL)
		Def_term = "ansi";
	/* Helios terminal attributes carry rows/columns in Min/Time. */
	if (hc_have_saved) {
		if (hc_saved.Min > 5) LINES = hc_saved.Min;
		if (hc_saved.Time > 4) COLS = hc_saved.Time;
	}
	/* Let LINES/COLUMNS from the environment override termcap. */
	if ((e = getenv("LINES")) != NULL && atoi(e) > 5)
		LINES = atoi(e);
	if ((e = getenv("COLUMNS")) != NULL && atoi(e) > 4)
		COLS = atoi(e);
	w = initscr();
	if (w == NULL)	/* BSD initscr: ERR, which is 0 there */
		return NULL;
	hc_tty = isatty(0);
	/* Helios curses (gettmode) sets raw input on stdin, but noecho()
	 * changes the terminal modes of fd 1 only: the console would still
	 * echo every key.  Switch the echo off on the input stream. */
	if (s != NULL) {
		Attributes a;

		if (GetAttributes(s, &a) >= 0) {
			RemoveAttribute(&a, ConsoleEcho);
			AddAttribute(&a, ConsoleRawInput);
			/* The I/O Server's console windows interpret
			 * escape sequences only in raw output mode. */
			AddAttribute(&a, ConsoleRawOutput);
			SetAttributes(s, &a);
		}
	}
	/* Cursor motion by TAB assumes tab stops every 8 columns; do not
	 * rely on that for the I/O Server consoles. */
	GT = FALSE;
	return w;
}

void hc_endwin(void)
{
	Stream *s = fdstream(0);

	endwin();
	fflush(stdout);
	if (hc_have_saved && s != NULL)
		SetAttributes(s, &hc_saved);
}

void hc_timeout(int ms)
{
	hc_delay = ms < 0 ? -1 : ms;
}

int hc_nodelay(int on)
{
	hc_delay = on ? 0 : -1;
	return OK;
}

/*
 * One byte from stdin, waiting at most ms milliseconds (-1: forever).
 * ERR if none arrived in time.  At the end of a redirected input (a
 * scripted session) the program ends, as there is nothing more to read.
 */
static int hc_byte(int ms)
{
	unsigned char b;
	word r;
	Stream *s;

	if (hc_npend > 0) {
		b = hc_pend[0];
		hc_npend--;
		memmove(hc_pend, hc_pend + 1, hc_npend);
		return b;
	}
	fflush(stdout);
	s = fdstream(0);
	if (s == NULL)
		return ERR;
	/* Helios Read() takes the timeout in microseconds as a signed
	 * 32-bit word: anything over about 2147 seconds would wrap to a
	 * negative value.  Wait without a timeout instead; a game that
	 * asks for such a long time limit cannot tell the difference. */
	if (ms < 0 || ms > 2000000L)
		r = Read(s, (byte *)&b, 1, -1);
	else
		r = Read(s, (byte *)&b, 1, (word)ms * 1000);
	if (r == 1)
		return b;
	if (hc_tty == 0 && ms != 0) {
		/* end of a redirected input */
		hc_endwin();
		exit(0);
	}
	return ERR;
}

static void hc_unread(int c)
{
	if (c >= 0 && hc_npend < (int)sizeof hc_pend)
		hc_pend[hc_npend++] = (unsigned char)c;
}

/* Decode the rest of a CSI sequence (after ESC [ or 0x9b). */
static int hc_csi(int hcsi)
{
	int c, n = 0, digits = 0, i;

	for (i = 0; i < 8; i++) {
		c = hc_byte(100);
		if (c == ERR)
			return ERR;
		if (c >= '0' && c <= '9') {
			n = n * 10 + (c - '0');
			digits = 1;
			continue;
		}
		if (c == ';' || c == ' ')
			continue;
		break;
	}
	switch (c) {
	case 'A': return KEY_UP;
	case 'B': return KEY_DOWN;
	case 'C': return KEY_RIGHT;
	case 'D': return KEY_LEFT;
	case 'H': return KEY_HOME;
	case 'F': return KEY_END;
	case '@': return KEY_IC;
	case 'z':	/* Helios: 1z undo, 2z end, 3z page up, 4z page down */
		switch (n) {
		case 2: return KEY_END;
		case 3: return KEY_PPAGE;
		case 4: return KEY_NPAGE;
		}
		return ERR;
	case '~':
		if (hcsi)	/* Helios: CSI n~ is function key n+1 */
			return digits && n <= 11 ? KEY_F(n + 1) : ERR;
		switch (n) {	/* VT220 / xterm */
		case 1: case 7: return KEY_HOME;
		case 2: return KEY_IC;
		case 3: return KEY_DC;
		case 4: case 8: return KEY_END;
		case 5: return KEY_PPAGE;
		case 6: return KEY_NPAGE;
		case 11: case 12: case 13: case 14: case 15:
			return KEY_F(n - 10);
		case 17: case 18: case 19: case 20: case 21:
			return KEY_F(n - 11);
		case 23: case 24:
			return KEY_F(n - 12);
		}
		return ERR;
	}
	return ERR;
}

int hc_getch(void)
{
	int c, d;

	c = hc_byte(hc_delay);
	if (c == ERR)
		return ERR;
	if (c == 0x9b)
		return hc_csi(1);
	if (c == 0x1b) {
		d = hc_byte(100);
		if (d == '[')
			return hc_csi(0);
		if (d == 'O') {
			d = hc_byte(100);
			if (d >= 'P' && d <= 'S')
				return KEY_F(d - 'P' + 1);
			switch (d) {
			case 'A': return KEY_UP;
			case 'B': return KEY_DOWN;
			case 'C': return KEY_RIGHT;
			case 'D': return KEY_LEFT;
			case 'H': return KEY_HOME;
			case 'F': return KEY_END;
			case 'M': return KEY_ENTER;
			}
			return ERR;
		}
		/* A lone ESC, or ESC + key (Alt-key); Frotz reads the
		 * second byte itself with nodelay. */
		hc_unread(d);
		return 0x1b;
	}
	if (c == 0x7f || c == 0x08)
		return KEY_BACKSPACE;
	return c;
}

int hc_attrset(int attrs)
{
	if (attrs & (A_STANDOUT | A_REVERSE | A_BOLD))
		wstandout(stdscr);
	else
		wstandend(stdscr);
	return OK;
}

int hc_setscrreg(int top, int bottom)
{
	hc_top = top;
	hc_bottom = bottom;
	return OK;
}

/* Scroll the region by n lines (n > 0: up), as ncurses scrl(). */
int hc_scrl(int n)
{
	int y, x, top = hc_top, bot = hc_bottom;

	if (bot < 0 || bot >= stdscr->_maxy)
		bot = stdscr->_maxy - 1;
	getyx(stdscr, y, x);

	for (; n > 0; n--) {
		wmove(stdscr, top, 0);
		wdeleteln(stdscr);
		wmove(stdscr, bot, 0);
		winsertln(stdscr);
	}
	for (; n < 0; n++) {
		wmove(stdscr, bot, 0);
		wdeleteln(stdscr);
		wmove(stdscr, top, 0);
		winsertln(stdscr);
	}
	wmove(stdscr, y, x);
	return OK;
}

int hc_mvaddnstr(int y, int x, const char *str, int n)
{
	wmove(stdscr, y, x);
	while (n-- > 0 && *str)
		waddch(stdscr, *str++);
	return OK;
}

int hc_beep(void)
{
	putchar('\007');
	fflush(stdout);
	return OK;
}

/*
 * Helios has no gettimeofday (sys/time.h declares it, no library has it).
 * Frotz only needs it for input timeouts: a monotonic clock from the
 * transputer's high priority timer (1 us ticks), widened past its 32-bit
 * wrap by accumulating the differences.
 */
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	static unsigned int last;
	static unsigned long sec, usec;
	static int started;
	unsigned int now = (unsigned int)_ldtimer(0);

	(void)tz;
	if (!started) {
		started = 1;
		last = now;
	}
	usec += now - last;
	last = now;
	sec += usec / 1000000;
	usec %= 1000000;
	tv->tv_sec = sec;
	tv->tv_usec = usec;
	return 0;
}
