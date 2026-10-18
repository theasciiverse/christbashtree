/*
 * christbashtree: Sergio Lepore's ChristBASHTree (tree-EN.sh and
 * tree-ES.sh, bash and tput, 2014 to 2018) as one small C program, for
 * the Asciiverse catalog. The scripts stay beside it in this repository.
 *
 *   christbashtree [en|es]
 *
 * A green tree of stars with a trunk and a greeting; coloured lights come
 * on and go off on the tree one after another, and the word CODE in the
 * greeting flashes in the colour of the newest light. The drawing, the
 * colours, the way lights are picked, kept and put out, and the pace are
 * the script's; added here: any key (Ctrl+C too) quits and puts the
 * terminal back, the picture is redrawn after a resize, it sits in the
 * middle of the window rather than at the top, nothing is drawn outside a
 * small window, and it runs on Windows (its console's VT mode).
 *
 * Like the original, released into the public domain (the Unlicense,
 * see LICENSE).
 */
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#define _DEFAULT_SOURCE
#define _DARWIN_C_SOURCE
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- the terminal ---- */

static char outbuf[16384];
static size_t outlen;
static volatile int stop;

static void
flush(void)
{
#ifdef _WIN32
	DWORD done;
	WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), outbuf, (DWORD)outlen, &done,
	    NULL);
#else
	size_t off = 0;
	while (off < outlen) {
		ssize_t n = write(STDOUT_FILENO, outbuf + off, outlen - off);
		if (n <= 0)
			break;
		off += (size_t)n;
	}
#endif
	outlen = 0;
}

static void
out(const char *s)
{
	size_t n = strlen(s);

	if (outlen + n > sizeof outbuf)
		flush();
	memcpy(outbuf + outlen, s, n);
	outlen += n;
}

#ifdef _WIN32
static DWORD in_mode, out_mode;

static BOOL WINAPI
on_ctrl(DWORD type)
{
	(void)type;
	stop = 1;
	return TRUE;
}

static void
term_start(void)
{
	HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);

	GetConsoleMode(in, &in_mode);
	GetConsoleMode(o, &out_mode);
	SetConsoleMode(o, out_mode | ENABLE_PROCESSED_OUTPUT |
	    ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	SetConsoleCtrlHandler(on_ctrl, TRUE);
}

static void
term_end(void)
{
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), in_mode);
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), out_mode);
}

static void
term_size(int *rows, int *cols)
{
	CONSOLE_SCREEN_BUFFER_INFO info;

	*rows = 24;
	*cols = 80;
	if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
		*rows = info.srWindow.Bottom - info.srWindow.Top + 1;
		*cols = info.srWindow.Right - info.srWindow.Left + 1;
	}
}

static int
key_pressed(void)
{
	int any = 0;

	while (_kbhit()) {
		_getch();
		any = 1;
	}
	return any;
}

static void
nap(int ms)
{
	Sleep((DWORD)ms);
}
#else
static struct termios saved;

static void
on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

static void
term_start(void)
{
	struct termios t;
	struct sigaction sa;

	if (tcgetattr(STDIN_FILENO, &saved) == 0) {
		t = saved;
		t.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
		t.c_iflag &= ~(IXON | ICRNL);
		t.c_cc[VMIN] = 1;
		t.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &t);
	}
	memset(&sa, 0, sizeof sa);
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = on_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
}

static void
term_end(void)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &saved);
}

static void
term_size(int *rows, int *cols)
{
	struct winsize ws;

	*rows = 24;
	*cols = 80;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 &&
	    ws.ws_col > 0) {
		*rows = ws.ws_row;
		*cols = ws.ws_col;
	}
}

static int
key_pressed(void)
{
	struct pollfd p = { STDIN_FILENO, POLLIN, 0 };
	char buf[64];

	if (poll(&p, 1, 0) <= 0)
		return 0;
	/* a key, or the terminal went away (read gives 0): both end it */
	if (read(STDIN_FILENO, buf, sizeof buf) < 0)
		return 0;
	return 1;
}

static void
nap(int ms)
{
	struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
	nanosleep(&ts, NULL);
}
#endif

/* ---- the picture ---- */

/* the script's two greetings (tree-EN.sh and tree-ES.sh) */
struct greeting {
	const char *line1, *before, *word, *after;
	int col1, col2;		/* where the lines start, from the middle */
};
static const struct greeting english = {
	"MERRY CHRISTMAS", "And lots of ", "CODE", " in ", -7, -11
};
static const struct greeting spanish = {
	"FELICES FIESTAS", "Y mucho ", "CODIGO", " en ", -7, -10
};
static const struct greeting *g = &english;

static int rows, cols;
static int base;		/* the screen row of the script's row 0 */
static int mid;			/* the script's middle column, cols / 2 */
static int year;

/*
 * What is on the tree: for each of its ten rows (the script's rows 2 to
 * 11) and each column from the middle (-9 to 9), -1 for a star or the
 * colour of the light there.
 */
static int tree[10][19];
static int word_colour = 1;	/* the colour CODE was last drawn in */

/* writes s at the script's row and column, cut to the window */
static void
put(int row, int col, const char *s)
{
	char buf[32];
	int y = base + row, n = (int)strlen(s), skip = 0;

	if (y < 0 || y >= rows || col >= cols || col + n <= 0)
		return;
	if (col < 0) {
		skip = -col;
		col = 0;
	}
	if (col + n - skip > cols)
		n = cols - col + skip;
	snprintf(buf, sizeof buf, "\033[%d;%dH", y + 1, col + 1);
	out(buf);
	for (int i = skip; i < n; i++) {
		char c[2] = { s[i], 0 };
		out(c);
	}
}

static void
colour(int c, int bold)
{
	char buf[24];

	/* tput sgr0, setaf and bold */
	snprintf(buf, sizeof buf, "\033[0;3%d%sm", c, bold ? ";1" : "");
	out(buf);
}

static void
draw_cell(int r, int dx)
{
	int c = tree[r][dx + 9];

	if (c < 0) {
		colour(2, 1);
		put(r + 2, mid + dx, "*");
	} else {
		colour(c, 1);
		put(r + 2, mid + dx, "o");
	}
}

static void
draw_word(void)
{
	int col = mid + g->col2 + (int)strlen(g->before);

	colour(word_colour, 1);
	put(15, col, g->word);
}

/* the whole picture, at the start and after a resize */
static void
draw_all(void)
{
	char buf[64];

	term_size(&rows, &cols);
	mid = cols / 2;
	/* the script draws rows 2 to 15; those 14 go in the middle */
	base = (rows > 14 ? (rows - 14) / 2 : 0) - 2;
	out("\033[0m\033[2J");
	for (int r = 0; r < 10; r++)
		for (int dx = -r; dx <= r; dx++)
			draw_cell(r, dx);
	colour(3, 0);
	put(12, mid - 1, "mWm");
	put(13, mid - 1, "mWm");
	colour(1, 1);
	put(14, mid + g->col1, g->line1);
	snprintf(buf, sizeof buf, "%s%s%s%d", g->before, g->word, g->after,
	    year);
	put(15, mid + g->col2, buf);
	draw_word();
}

/*
 * Waits ms milliseconds, watching for a key, a signal and a resize.
 * Returns 1 to stop.
 */
static int
wait_ms(int ms)
{
	flush();
	while (ms > 0) {
		int r, c, step = ms < 10 ? ms : 10;
		if (stop || key_pressed())
			return 1;
		term_size(&r, &c);
		if (r != rows || c != cols) {
			draw_all();
			flush();
		}
		nap(step);
		ms -= step;
	}
	return stop || key_pressed();
}

int
main(int argc, char **argv)
{
	/* the lights of the two rounds: their rows and columns from the middle */
	int line[3][36], column[3][36], lit[3][36];
	int k = 1, col = 0;
	time_t now = time(NULL);

	if (argc > 1 && strcmp(argv[1], "es") == 0)
		g = &spanish;
	else if (argc > 1 && strcmp(argv[1], "en") != 0) {
		fprintf(stderr, "usage: christbashtree [en|es]\n");
		return 2;
	}
	/* "And lots of CODE in" next year */
	year = localtime(&now)->tm_year + 1900 + 1;
	srand((unsigned)now ^ ((unsigned)clock() << 8));
	memset(lit, 0, sizeof lit);
	for (int r = 0; r < 10; r++)
		for (int dx = 0; dx < 19; dx++)
			tree[r][dx] = -1;

	term_start();
	/* alternate screen, cursor hidden, no wrapping at the right edge */
	out("\033[?1049h\033[?25l\033[?7l");
	draw_all();

	for (;;) {
		for (int i = 1; i <= 35; i++) {
			int li, start, co;

			/*
			 * Turn off the lights: the script puts out the other
			 * round's light only in round 2, so lights lit in
			 * round 2 stay on until a round 1 light lands on them
			 * and is put out
			 */
			if (k > 1 && lit[k - 1][i]) {
				tree[line[k - 1][i] - 2][column[k - 1][i] + 9] = -1;
				draw_cell(line[k - 1][i] - 2, column[k - 1][i]);
				lit[k - 1][i] = 0;
			}
			/* a light somewhere on rows 3 to 11, between the stars */
			li = rand() % 9 + 3;
			start = mid - li + 2;
			co = rand() % (li - 2) * 2 + 1 + start - mid;
			tree[li - 2][co + 9] = col;
			draw_cell(li - 2, co);
			line[k][i] = li;
			column[k][i] = co;
			lit[k][i] = 1;
			word_colour = col;
			col = (col + 1) % 8;
			/* the flashing word, a letter at a time */
			if (wait_ms(15))
				goto done;
			for (size_t l = 1; l <= strlen(g->word); l++) {
				char letter[2] = { g->word[l - 1], 0 };
				colour(word_colour, 1);
				put(15, mid + g->col2 + (int)strlen(g->before) +
				    (int)l - 1, letter);
				if (wait_ms(10))
					goto done;
			}
		}
		k = k % 2 + 1;
	}
done:
	out("\033[0m\033[2J\033[?7h\033[?25h\033[?1049l");
	flush();
	term_end();
	return 0;
}
