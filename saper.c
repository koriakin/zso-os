#include <stdint.h>
#include <stdbool.h>

#include "rand.h"

/*-----------------------------------------------------------------------------
 *  These should be defined somewhere else...
 *-----------------------------------------------------------------------------*/

#define GREY 0
#define RED 1
#define BLUE 2
#define GREEN 3
#define PURPLE 4
#define MAROON 5
#define CYAN 6
#define YELLOW 7

#define KEY_DOWNARROW 0
#define KEY_UPARROW 1
#define KEY_LEFTARROW 2
#define KEY_RIGHTARROW 3
#define KEY_SPACE 4
#define KEY_ENTER 5
#define KEY_ESC 6

struct window {
        int top, left;
        int width, height;
        int cursor_x, cursor_y;
        uint8_t attr; /* background and text color */
};

struct keyboard_receiver;

void printf(struct window *, const char *, ...);
void gotoxy(struct window *, int x, int y);
void setattr(struct window *, uint8_t attr);
unsigned gettime(); /* zwraca czas w milisekundach od uruchomienia systemu */
int getkey_timeout(struct keyboard_receiver *, unsigned timeout);

/*-----------------------------------------------------------------------------
 *  Saper Types and Constants
 *-----------------------------------------------------------------------------*/

/* expert mode only! */
#define BOARD_H 16
#define BOARD_W 30
#define MINES 99

#define WIN_H (BOARD_H + 3 /* border */ + 1 /* status line */)
#define WIN_W (BOARD_W + 2 /* border */)

#define MILLISEC_PER_SEC 1000
#define REFRESH_TIME (1 * MILLISEC_PER_SEC)
#define TIME_LIMIT (999 * REFRESH_TIME)

typedef enum {REVEALED, FLAGGED, HIDDEN} state_t;

#define FLAG_KEY KEY_SPACE
#define REVEAL_KEY KEY_ENTER
#define EXIT_KEY KEY_ESC

#define HIDDEN_SYM '.'
#define REVEALED_SYM ' '
#define BOMB_SYM '*'
#define FLAG_SYM 'P'
#define int_to_sym(x) (x == 0 ? REVEALED_SYM : x - '0')

static const uint8_t int_to_color[] = {GREY, BLUE, GREEN, RED, PURPLE, MAROON, CYAN, YELLOW, GREY};

/*-----------------------------------------------------------------------------
 *  Global Data
 *-----------------------------------------------------------------------------*/

static unsigned start_time;
static bool has_bomb[BOARD_W][BOARD_H];
static state_t state[BOARD_W][BOARD_H];
static int board_top, board_left;
static int mines;
static struct window *window;
static struct keyboard_receiver *keyboard;

#define board_right() (board_left + BOARD_W - 1)
#define board_bottom() (board_top + BOARD_H + 1)
#define status_line() (board_top - 2)
#define board_col(x) (x - board_left)
#define board_row(y) (y - board_top)
#define cur_col() board_col(window->cursor_x)
#define cur_row() board_row(window->cursor_y)

/*-----------------------------------------------------------------------------
 *  Utils
 *-----------------------------------------------------------------------------*/

static bool check_board_coord(int col, int row)
{
        return 0 <= col && col < BOARD_W && 0 <= row && row < BOARD_H;
}

static const int dx[] = {0, 1, 1, 1, 0, -1, -1, -1}, dy[] = {-1, -1, 0, 1, 1, 1, 0, -1};

#define for_each_neigh(x, y, nx, ny) for (i = 0, nx = x + dx[0], ny = y + dy[0]; \
                i < 8; ++i, nx = x + dx[i], y = y + dy[i]) if (check_board_coord(nx, ny))

static int neighbour_bombs_num(int col, int row)
{
        int i, res, ncol, nrow;
        res = 0;
        for_each_neigh(col, row, ncol, nrow)
                res += has_bomb[ncol][nrow];
        return res;
}

/*-----------------------------------------------------------------------------
 *  Display
 *-----------------------------------------------------------------------------*/

static int last_cursor_x, last_cursor_y;
static void save_cursor()
{
        last_cursor_x = window->cursor_x;
        last_cursor_y = window->cursor_y;
}

static void restore_cursor()
{
        window->cursor_x = last_cursor_x;
        window->cursor_y = last_cursor_y;
        gotoxy(window, last_cursor_x, last_cursor_y);
}

static unsigned get_digits_num(unsigned n)
{
        int i, res;
        for (res = 1; n >= 10; n /= 10)
                ++res;
        return res;
}

static void print_number_zero_padded(unsigned n, int len)
{
        unsigned digits_num, i;
        digits_num = get_digits_num(n);
        for (i = 0; i < len - digits_num; ++i)
                printf(window, "0");
        printf(window, "%d", n);
}

static void print_char(int x, int y, char c)
{
        char cc[2];
        save_cursor();
        gotoxy(window, x, y);
        cc[0] = c;
        cc[1] = '\0';
        printf(window, cc);
        restore_cursor();
}

static void print_char_on_board(int col, int row, char c)
{
        print_char(board_left + col, board_top + row, c);
}

static void print_status_line_msg(const char* msg)
{
        save_cursor();
        gotoxy(window, board_left + BOARD_W / 2, status_line());
        setattr(window, RED);
        printf(window, msg);
        restore_cursor();
}

static void display_mines_num()
{
        save_cursor();
        gotoxy(window, board_left, status_line());
        setattr(window, RED);
        print_number_zero_padded(mines, 2);
        restore_cursor();
}

static void display_time()
{
        unsigned elapsed;
        save_cursor();
        gotoxy(window, board_left + BOARD_W - 3, status_line());
        setattr(window, RED);
        elapsed = ((gettime() - start_time) / MILLISEC_PER_SEC) % 999;
        print_number_zero_padded(elapsed, 3);
        restore_cursor();
}

static void display_cell(int col, int row)
{
        int nb;
        uint8_t color;
        char c;
        if (state[col][row] == HIDDEN) {
                color = GREY;
                c = HIDDEN_SYM;
        } else if (state[col][row] == FLAGGED) {
                color = RED;
                c = FLAG_SYM;
        } else {
                nb = neighbour_bombs_num(col, row);
                col = int_to_color[nb];
                c = int_to_sym(nb);
        }
        setattr(window, color);
        print_char_on_board(col, row, c);
}


static void init_display_game()
{
        int x, y;

        /* display border */
        setattr(window, GREY);
        print_char(board_left - 1, status_line() - 1, '/');
        print_char(board_right() + 1, status_line() - 1, '\\');
        print_char(board_left - 1, board_bottom() + 1, '\\');
        print_char(board_right() + 1, board_bottom() + 1, '/');
        for (x = 0; x < BOARD_W; ++x) {
                print_char(board_left + x, status_line() - 1, '-');
                print_char(board_left + x, status_line() + 1, '-');
                print_char(board_left + x, board_bottom() + 1, '-');
        }
        for (y = -2; y < BOARD_H; ++y) {
                print_char(board_left - 1, board_top + y, '|');
                print_char(board_right() + 1, board_top + y, '|');
        }

        for (x = 0; x < BOARD_W; ++x)
                for (y = 0; y < BOARD_H; ++y)
                        display_cell(x, y);

        display_mines_num();

        display_time();
}

/*-----------------------------------------------------------------------------
 *  Game
 *-----------------------------------------------------------------------------*/

static bool good_solution()
{
        int col, row;
        for (col = 0; col < BOARD_W; ++col)
                for (row = 0; row < BOARD_H; ++row)
                        if (has_bomb[col][row] != (state[col][row] == FLAGGED))
                                return false;
        return true;
}

static void set_flag()
{
        state_t *st;
        st = &state[cur_col()][cur_row()];
        if (*st == HIDDEN) {
                *st = FLAGGED;
                --mines;
        }
        else if (*st == FLAGGED) {
                *st = HIDDEN;
                ++mines;
        }
        display_cell(cur_col(), cur_row());
        display_mines_num();
}


static void reveal_cell(int col, int row)
{
        int i, ncol, nrow;
        if (state[col][row] != HIDDEN)
                return;
        state[col][row] = REVEALED;
        display_cell(col, row);
        if (neighbour_bombs_num(col, row) == 0)
                for_each_neigh(col, row, ncol, nrow)
                        reveal_cell(ncol, nrow);
}

/* prevents the cursor from leaving the board */
static void shift_cursor_on_board(int dx, int dy)
{
        int nx, ny;
        nx = window->cursor_x + dx;
        ny = window->cursor_y + dy;
        if (!check_board_coord(board_col(nx), board_row(ny)))
                return;
        window->cursor_x = nx;
        window->cursor_y = ny;
        gotoxy(window, nx, ny);
}

static int init_game()
{
        int x, y;

        if (window->width < WIN_W || window->height < WIN_H)
                return -1;
        board_top = window->top + (window->height - WIN_H) / 2;
        board_left = window->left + (window->width - WIN_W) / 2;

        start_time = gettime();

        for (x = 0; x < BOARD_W; ++x)
                for (y = 0; y < BOARD_H; ++y) {
                        state[x][y] = HIDDEN;
                        has_bomb[x][y] = false;
                }

        srand(start_time);
        mines = 0;
        while (mines < MINES) {
                x = rand() % BOARD_W;
                y = rand() & BOARD_H;
                if (has_bomb[x][y]) continue;
                has_bomb[x][y] = true;
                ++mines;
        }

        return 0;
}

int play(struct window *win, struct keyboard_receiver *key_rec)
{
        int key;
        unsigned cur_time, next_refresh;
        window = win;
        keyboard = key_rec;
        if (init_game() != 0)
                return -1;
        init_display_game();
        window->cursor_x = board_left;
        window->cursor_y = board_top;
        shift_cursor_on_board(BOARD_W / 2, BOARD_H / 2);
        while (true) {
                cur_time = gettime();
                next_refresh = cur_time - cur_time % REFRESH_TIME + REFRESH_TIME;
                if (next_refresh - start_time > TIME_LIMIT)
                        goto end;
                key = getkey_timeout(key_rec, next_refresh - cur_time);
                display_time();
                switch (key) {
                        case KEY_DOWNARROW:
                                shift_cursor_on_board(0, 1);
                                break;
                        case KEY_UPARROW:
                                shift_cursor_on_board(0, -1);
                                break;
                        case KEY_LEFTARROW:
                                shift_cursor_on_board(-1, 0);
                                break;
                        case KEY_RIGHTARROW:
                                shift_cursor_on_board(1, 0);
                                break;

                        case FLAG_KEY:
                                set_flag();
                                if (mines == 0)
                                        goto end;
                                break;

                        case REVEAL_KEY:
                                if (has_bomb[cur_col()][cur_row()])
                                        goto end;
                                reveal_cell(cur_col(), cur_row());
                                break;

                        case EXIT_KEY:
                                goto end;

                        default:
                                /* do nothing */
                                break;

                }
        }
end:
        print_status_line_msg(good_solution() ? "you won!" : "you lost!");
        key = getkey_timeout(key_rec, 3 * REFRESH_TIME);
        return 0;
}
