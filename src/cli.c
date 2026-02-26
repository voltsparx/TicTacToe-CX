#include "cli.h"
#include "app_meta.h"

#include <ctype.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
    #include <conio.h>
    #include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
    #include <sys/select.h>
    #include <termios.h>
    #include <unistd.h>
#endif

static ColorTheme current_theme = THEME_DEFAULT;

typedef struct {
    const char* h;
    const char* v;
    const char* tl;
    const char* tm;
    const char* tr;
    const char* ml;
    const char* mm;
    const char* mr;
    const char* bl;
    const char* bm;
    const char* br;
    const char* dot;
    const char* nav_hint;
} CliGlyphs;

static const CliGlyphs GLYPHS_UNICODE = {
    "─", "│",
    "┌", "┬", "┐",
    "├", "┼", "┤",
    "└", "┴", "┘",
    "•",
    "Use ↑/↓ and Enter"
};

static const CliGlyphs GLYPHS_ASCII = {
    "-", "|",
    "+", "+", "+",
    "+", "+", "+",
    "+", "+", "+",
    "*",
    "Use Up/Down and Enter"
};

static const CliGlyphs* g_glyphs = &GLYPHS_UNICODE;
static bool g_use_live_render = true;

#define CLI_MENU_MAX_OPTIONS 12
#define CLI_MENU_MAX_TEXT 128

typedef struct {
    bool active;
    char menu_id[32];
    int option_count;
    int selected_index;
    char cached_titles[CLI_MENU_MAX_OPTIONS][CLI_MENU_MAX_TEXT];
    char cached_subtitles[CLI_MENU_MAX_OPTIONS][CLI_MENU_MAX_TEXT];
} LiveMenuState;

static LiveMenuState g_live_menu = {0};

static void print_theme_colors(void);

static void delay_seconds(unsigned int seconds) {
#ifdef _WIN32
    Sleep(seconds * 1000U);
#else
    sleep(seconds);
#endif
}

static bool cell_is_on_win_line(const Game* game, uint8_t row, uint8_t col) {
    if (!game || game->state != GAME_STATE_WIN || game->win_line[0] < 0) {
        return false;
    }

    const int sr = game->win_line[0];
    const int sc = game->win_line[1];
    const int er = game->win_line[2];
    const int ec = game->win_line[3];
    const int dr = (er > sr) - (er < sr);
    const int dc = (ec > sc) - (ec < sc);

    int r = sr;
    int c = sc;
    while (1) {
        if (r == (int)row && c == (int)col) {
            return true;
        }
        if (r == er && c == ec) {
            break;
        }
        r += dr;
        c += dc;
    }

    return false;
}

static int count_digits(unsigned int value) {
    int digits = 1;
    while (value >= 10U) {
        value /= 10U;
        digits++;
    }
    return digits;
}

static bool locale_supports_utf8(void) {
    const char* locale_name = setlocale(LC_CTYPE, NULL);
    if (!locale_name) {
        return false;
    }

    char lowered[64];
    size_t j = 0;
    for (size_t i = 0; locale_name[i] != '\0' && j + 1 < sizeof(lowered); i++) {
        lowered[j++] = (char)tolower((unsigned char)locale_name[i]);
    }
    lowered[j] = '\0';

    return strstr(lowered, "utf-8") != NULL ||
           strstr(lowered, "utf8") != NULL;
}

static bool env_flag_enabled(const char* name) {
    const char* value = getenv(name);
    if (!value) {
        return false;
    }

    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "YES") == 0;
}

#ifndef _WIN32
static bool terminal_supports_live_render(void) {
    const char* term = getenv("TERM");
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
        return false;
    }
    if (!term || term[0] == '\0') {
        return false;
    }
    if (strcmp(term, "dumb") == 0) {
        return false;
    }
    return true;
}
#endif

void cli_init_terminal(void) {
    (void)setlocale(LC_ALL, "");
    g_use_live_render = true;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    bool vt_ok = false;
    if (out != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(out, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            vt_ok = SetConsoleMode(out, mode) != 0;
        }
    }
    if (!vt_ok) {
        g_use_live_render = false;
    }
#else
    g_use_live_render = terminal_supports_live_render();
#endif

    if (env_flag_enabled("TICTACTOE_NO_LIVE")) {
        g_use_live_render = false;
    }

    const char* force_ascii = getenv("TICTACTOE_ASCII");
    if (force_ascii && (strcmp(force_ascii, "1") == 0 || strcmp(force_ascii, "true") == 0)) {
        g_glyphs = &GLYPHS_ASCII;
        return;
    }

    g_glyphs = locale_supports_utf8() ? &GLYPHS_UNICODE : &GLYPHS_ASCII;
    if (!g_use_live_render) {
        g_glyphs = &GLYPHS_ASCII;
    }
}

static void print_board_border(uint8_t n, uint8_t cell_width, int left_padding,
                               const char* left, const char* middle, const char* right) {
    for (int s = 0; s < left_padding; s++) {
        putchar(' ');
    }

    printf("%s", left);
    for (uint8_t col = 0; col < n; col++) {
        for (uint8_t k = 0; k < cell_width; k++) {
            printf("%s", g_glyphs->h);
        }
        printf("%s", (col == n - 1) ? right : middle);
    }
    printf("\n");
}

static const char* theme_name(int theme) {
    switch (theme) {
        case THEME_DARK:
            return "Dark";
        case THEME_LIGHT:
            return "Light";
        case THEME_RETRO:
            return "Retro";
        default:
            return "Default";
    }
}

#ifndef _WIN32
static int read_char_with_timeout_ms(int timeout_ms) {
    fd_set readfds;
    struct timeval timeout;

    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    if (ready <= 0) {
        return -1;
    }

    unsigned char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) != 1) {
        return -1;
    }
    return (int)ch;
}

static CliKey read_stream_key_fallback(void) {
    int ch = getchar();
    if (ch == EOF) {
        return CLI_KEY_ESCAPE;
    }

    if (ch == '\n' || ch == '\r') return CLI_KEY_ENTER;
    if (ch == 'w' || ch == 'W') return CLI_KEY_UP;
    if (ch == 's' || ch == 'S') return CLI_KEY_DOWN;
    if (ch == 'a' || ch == 'A') return CLI_KEY_LEFT;
    if (ch == 'd' || ch == 'D') return CLI_KEY_RIGHT;

    if (ch == 27) {
        int second = getchar();
        if (second == '[') {
            int third = getchar();
            switch (third) {
                case 'A':
                    return CLI_KEY_UP;
                case 'B':
                    return CLI_KEY_DOWN;
                case 'C':
                    return CLI_KEY_RIGHT;
                case 'D':
                    return CLI_KEY_LEFT;
                default:
                    return CLI_KEY_ESCAPE;
            }
        }
        return CLI_KEY_ESCAPE;
    }

    return CLI_KEY_NONE;
}
#endif

static void print_menu_header(const char* title) {
    printf(ANSI_BOLD "=======================================\n");
    printf("            " ANSI_YELLOW " %s " ANSI_RESET "\n", title ? title : "");
    printf(ANSI_BOLD "=======================================\n\n");
}

static void print_menu_option(const char* title, const char* subtitle, bool selected) {
    if (selected) {
        printf(ANSI_BG_CYAN ANSI_BLACK ANSI_BOLD "  %-35s  " ANSI_RESET "\n", title ? title : "");
        if (subtitle && subtitle[0] != '\0') {
            printf(ANSI_BRIGHT_CYAN "    %s" ANSI_RESET "\n", subtitle);
        }
    } else {
        printf(ANSI_WHITE "  %-35s  " ANSI_RESET "\n", title ? title : "");
        if (subtitle && subtitle[0] != '\0') {
            printf(ANSI_GRAY "    %s" ANSI_RESET "\n", subtitle);
        }
    }
    printf("\n");
}

static void print_menu_footer(void) {
    printf(ANSI_BOLD "=======================================\n");
    printf("  " ANSI_CYAN "%s" ANSI_RESET "\n", g_glyphs->nav_hint);
}

static int menu_option_row(int index) {
    return 5 + (index * 3);
}

static int menu_footer_row(int option_count) {
    return 5 + (option_count * 3);
}

static void copy_text_limited(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    (void)snprintf(dst, dst_size, "%s", src);
}

static void render_option_at_row(int row, const char* title, const char* subtitle, bool selected) {
    if (!g_use_live_render) {
        return;
    }

    printf("\033[%d;1H\033[2K", row);
    if (selected) {
        printf(ANSI_BG_CYAN ANSI_BLACK ANSI_BOLD "  %-35s  " ANSI_RESET, title ? title : "");
    } else {
        printf(ANSI_WHITE "  %-35s  " ANSI_RESET, title ? title : "");
    }

    printf("\033[%d;1H\033[2K", row + 1);
    if (subtitle && subtitle[0] != '\0') {
        if (selected) {
            printf(ANSI_BRIGHT_CYAN "    %s" ANSI_RESET, subtitle);
        } else {
            printf(ANSI_GRAY "    %s" ANSI_RESET, subtitle);
        }
    }

    printf("\033[%d;1H\033[2K", row + 2);
}

static void cache_menu_entries(const char* const options[], const char* const subtitles[], int option_count) {
    for (int i = 0; i < option_count && i < CLI_MENU_MAX_OPTIONS; i++) {
        copy_text_limited(g_live_menu.cached_titles[i], sizeof(g_live_menu.cached_titles[i]), options ? options[i] : "");
        copy_text_limited(g_live_menu.cached_subtitles[i], sizeof(g_live_menu.cached_subtitles[i]), subtitles ? subtitles[i] : "");
    }
    for (int i = option_count; i < CLI_MENU_MAX_OPTIONS; i++) {
        g_live_menu.cached_titles[i][0] = '\0';
        g_live_menu.cached_subtitles[i][0] = '\0';
    }
}

static void render_live_menu(const char* menu_id,
                             const char* title,
                             const char* const options[],
                             const char* const subtitles[],
                             int option_count,
                             int selected_index) {
    if (!menu_id || !title || !options || option_count <= 0) {
        return;
    }

    if (option_count > CLI_MENU_MAX_OPTIONS) {
        option_count = CLI_MENU_MAX_OPTIONS;
    }

    if (selected_index < 0 || selected_index >= option_count) {
        selected_index = 0;
    }

    const bool same_menu = g_live_menu.active &&
                           strcmp(g_live_menu.menu_id, menu_id) == 0 &&
                           g_live_menu.option_count == option_count;

    if (!g_use_live_render || !same_menu) {
        cli_clear_screen();
        print_theme_colors();
        print_menu_header(title);

        for (int i = 0; i < option_count; i++) {
            print_menu_option(options[i], subtitles ? subtitles[i] : "", i == selected_index);
        }

        print_menu_footer();
        if (g_use_live_render) {
            printf("\033[?25l");
        }
        fflush(stdout);

        copy_text_limited(g_live_menu.menu_id, sizeof(g_live_menu.menu_id), menu_id);
        g_live_menu.active = true;
        g_live_menu.option_count = option_count;
        g_live_menu.selected_index = selected_index;
        cache_menu_entries(options, subtitles, option_count);
        return;
    }

    bool any_updates = false;
    for (int i = 0; i < option_count; i++) {
        const char* new_title = options[i] ? options[i] : "";
        const char* new_subtitle = (subtitles && subtitles[i]) ? subtitles[i] : "";

        if (strcmp(g_live_menu.cached_titles[i], new_title) != 0 ||
            strcmp(g_live_menu.cached_subtitles[i], new_subtitle) != 0) {
            render_option_at_row(menu_option_row(i), new_title, new_subtitle, i == selected_index);
            copy_text_limited(g_live_menu.cached_titles[i], sizeof(g_live_menu.cached_titles[i]), new_title);
            copy_text_limited(g_live_menu.cached_subtitles[i], sizeof(g_live_menu.cached_subtitles[i]), new_subtitle);
            any_updates = true;
        }
    }

    if (g_live_menu.selected_index != selected_index) {
        const int old_idx = g_live_menu.selected_index;
        if (old_idx >= 0 && old_idx < option_count) {
            render_option_at_row(menu_option_row(old_idx),
                                 options[old_idx],
                                 subtitles ? subtitles[old_idx] : "",
                                 false);
        }
        render_option_at_row(menu_option_row(selected_index),
                             options[selected_index],
                             subtitles ? subtitles[selected_index] : "",
                             true);
        any_updates = true;
    }

    g_live_menu.selected_index = selected_index;
    if (any_updates) {
        printf("\033[%d;1H", menu_footer_row(option_count) + 2);
        fflush(stdout);
    }
}

void cli_menu_invalidate(void) {
    g_live_menu.active = false;
    g_live_menu.menu_id[0] = '\0';
    g_live_menu.option_count = 0;
    g_live_menu.selected_index = 0;
    if (g_use_live_render) {
        printf("\033[?25h");
    }
    fflush(stdout);
}

CliKey cli_read_menu_key(void) {
#ifdef _WIN32
    int ch = _getch();

    if (ch == 0 || ch == 224) {
        int ext = _getch();
        switch (ext) {
            case 72:
                return CLI_KEY_UP;
            case 80:
                return CLI_KEY_DOWN;
            case 75:
                return CLI_KEY_LEFT;
            case 77:
                return CLI_KEY_RIGHT;
            default:
                return CLI_KEY_NONE;
        }
    }

    if (ch == '\r') return CLI_KEY_ENTER;
    if (ch == 27) return CLI_KEY_ESCAPE;
    if (ch == 'w' || ch == 'W') return CLI_KEY_UP;
    if (ch == 's' || ch == 'S') return CLI_KEY_DOWN;
    if (ch == 'a' || ch == 'A') return CLI_KEY_LEFT;
    if (ch == 'd' || ch == 'D') return CLI_KEY_RIGHT;
    return CLI_KEY_NONE;
#else
    struct termios oldt;
    struct termios raw;
    CliKey key = CLI_KEY_NONE;

    if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
        return read_stream_key_fallback();
    }

    raw = oldt;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return read_stream_key_fallback();
    }

    unsigned char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == '\n' || ch == '\r') {
            key = CLI_KEY_ENTER;
        } else if (ch == 27) {
            int second = read_char_with_timeout_ms(25);
            if (second == '[') {
                int third = read_char_with_timeout_ms(25);
                switch (third) {
                    case 'A':
                        key = CLI_KEY_UP;
                        break;
                    case 'B':
                        key = CLI_KEY_DOWN;
                        break;
                    case 'C':
                        key = CLI_KEY_RIGHT;
                        break;
                    case 'D':
                        key = CLI_KEY_LEFT;
                        break;
                    default:
                        key = CLI_KEY_ESCAPE;
                        break;
                }
            } else {
                key = CLI_KEY_ESCAPE;
            }
        } else if (ch == 'w' || ch == 'W') {
            key = CLI_KEY_UP;
        } else if (ch == 's' || ch == 'S') {
            key = CLI_KEY_DOWN;
        } else if (ch == 'a' || ch == 'A') {
            key = CLI_KEY_LEFT;
        } else if (ch == 'd' || ch == 'D') {
            key = CLI_KEY_RIGHT;
        }
    }

    (void)tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return key;
#endif
}

void cli_clear_screen(void) {
    printf("\033[2J\033[H");
}

static void print_theme_colors(void) {
    switch (current_theme) {
        case THEME_DARK:
            printf("\033[97m");
            break;
        case THEME_LIGHT:
            printf("\033[30m");
            break;
        case THEME_RETRO:
            printf("\033[32m");
            break;
        default:
            printf(ANSI_WHITE);
            break;
    }
}

void cli_set_theme(ColorTheme theme) {
    current_theme = theme;
}

void cli_print_title(void) {
    cli_clear_screen();
    print_theme_colors();

    printf(COLOR_TITLE "\n  %s " ANSI_YELLOW "v%s\n\n" ANSI_RESET, APP_NAME, APP_VERSION);
}

void cli_print_main_menu(int selected_index) {
    static const char* options[] = {
        "Play vs AI",
        "Two Player Local",
        "LAN Multiplayer",
        "Internet Multiplayer",
        "High Scores",
        "Settings",
        "About",
        "Quit Game"
    };
    static const char* subtitles[] = {
        "Easy, Medium, Hard",
        "Play on one keyboard",
        "Host or join over LAN",
        "Cloudflared tunneled sessions",
        "See your match stats",
        "Board, colors, timer, sound",
        "Game info and credits",
        "Exit TicTacToe-CX"
    };
    const int option_count = (int)(sizeof(options) / sizeof(options[0]));

    if (selected_index < 0 || selected_index >= option_count) {
        selected_index = 0;
    }

    render_live_menu("main", "TICTACTOE-CX MAIN MENU", options, subtitles, option_count, selected_index);
}

void cli_print_game_menu(int selected_index) {
    static const char* options[] = {
        "Easy AI",
        "Medium AI",
        "Hard AI",
        "Back"
    };
    static const char* subtitles[] = {
        "Random moves",
        "Defensive and opportunistic",
        "Minimax (hardest)",
        "Return to main menu"
    };
    const int option_count = (int)(sizeof(options) / sizeof(options[0]));

    if (selected_index < 0 || selected_index >= option_count) {
        selected_index = 0;
    }

    render_live_menu("game_mode", "GAME MODE", options, subtitles, option_count, selected_index);
}

void cli_print_network_menu(int selected_index) {
    static const char* options[] = {
        "Host Game",
        "Join Game",
        "Back"
    };
    static const char* subtitles[] = {
        "Create a LAN match",
        "Connect to a host",
        "Return to main menu"
    };
    const int option_count = (int)(sizeof(options) / sizeof(options[0]));

    if (selected_index < 0 || selected_index >= option_count) {
        selected_index = 0;
    }

    render_live_menu("network", "LAN MULTIPLAYER", options, subtitles, option_count, selected_index);
}

void cli_print_internet_menu(int selected_index) {
    static const char* options[] = {
        "Host Internet Game",
        "Join Internet Game",
        "Back"
    };
    static const char* subtitles[] = {
        "Expose your local port using cloudflared",
        "Use a cloudflared hostname/link",
        "Return to main menu"
    };
    const int option_count = (int)(sizeof(options) / sizeof(options[0]));

    if (selected_index < 0 || selected_index >= option_count) {
        selected_index = 0;
    }

    render_live_menu("internet", "INTERNET MULTIPLAYER", options, subtitles, option_count, selected_index);
}

void cli_print_board(Game* game) {
    if (!game || game->size == 0) {
        return;
    }

    cli_clear_screen();
    print_theme_colors();
    printf(COLOR_TITLE "\n  %s " ANSI_YELLOW "v%s\n" ANSI_RESET, APP_NAME, APP_VERSION);
    printf(ANSI_GRAY "  Live Match View\n\n" ANSI_RESET);

    const uint8_t n = game->size;
    const uint8_t cell_width = 7;
    const int row_digits = count_digits(n);
    const int left_padding = row_digits + 2;

    print_board_border(n, cell_width, left_padding, g_glyphs->tl, g_glyphs->tm, g_glyphs->tr);

    for (uint8_t i = 0; i < n; i++) {
        printf(" %*u ", row_digits, (unsigned int)(i + 1));
        for (uint8_t j = 0; j < n; j++) {
            char c = game_get_cell_char(game, i, j);

            printf("%s", g_glyphs->v);
            if (cell_is_on_win_line(game, i, j) && c != ' ') {
                printf(COLOR_WIN "   %c   " ANSI_RESET, c);
            } else if (c == game->symbol_x) {
                printf(COLOR_X "   %c   " ANSI_RESET, c);
            } else if (c == game->symbol_o) {
                printf(COLOR_O "   %c   " ANSI_RESET, c);
            } else if (cell_is_on_win_line(game, i, j)) {
                printf(COLOR_WIN "   %s   " ANSI_RESET, g_glyphs->dot);
            } else {
                printf("       ");
            }
        }
        printf("%s\n", g_glyphs->v);

        if (i == n - 1) {
            print_board_border(n, cell_width, left_padding, g_glyphs->bl, g_glyphs->bm, g_glyphs->br);
        } else {
            print_board_border(n, cell_width, left_padding, g_glyphs->ml, g_glyphs->mm, g_glyphs->mr);
        }
    }

    printf("\n");
    for (int s = 0; s < left_padding + 1; s++) {
        putchar(' ');
    }
    for (uint8_t j = 0; j < n; j++) {
        const unsigned int col_num = (unsigned int)(j + 1);
        const int col_digits = count_digits(col_num);
        const int free_space = (int)cell_width - col_digits;
        const int left_space = free_space / 2;
        const int right_space = free_space - left_space;

        for (int s = 0; s < left_space; s++) {
            putchar(' ');
        }
        printf("%u", col_num);
        for (int s = 0; s < right_space; s++) {
            putchar(' ');
        }

        if (j != n - 1) {
            putchar(' ');
        }
    }
    printf("\n");

    Player current = game->current_player;
    if (current == PLAYER_X) {
        printf("  " COLOR_X "▶ %c's Turn" ANSI_RESET, game->symbol_x);
    } else {
        printf("  " COLOR_O "▶ %c's Turn" ANSI_RESET, game->symbol_o);
    }

    printf("\n");
    printf("  " ANSI_GRAY "Move format: 23 or 2 3 | Press Q to return to menu" ANSI_RESET "\n");
    fflush(stdout);
}

void cli_print_move_prompt(void) {
    print_theme_colors();
    printf("\n  " COLOR_PROMPT "Enter move (23 or 2 3): " ANSI_RESET);
}

void cli_print_game_over(Game* game) {
    printf("\n");
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game_get_winner(game);
        if (winner == PLAYER_X) {
            printf(ANSI_BOLD "  +======================================+\n");
            printf(        "  |      " COLOR_X "  %c WINS!  " ANSI_BOLD "               |\n", game->symbol_x);
            printf(        "  +======================================+" ANSI_RESET "\n");
        } else {
            printf(ANSI_BOLD "  +======================================+\n");
            printf(        "  |      " COLOR_O "  %c WINS!  " ANSI_BOLD "               |\n", game->symbol_o);
            printf(        "  +======================================+" ANSI_RESET "\n");
        }
    } else if (game->state == GAME_STATE_DRAW) {
        printf(ANSI_BOLD "  +======================================+\n");
        printf(        "  |      " COLOR_DRAW "  IT'S A DRAW!  " ANSI_BOLD "               |\n");
        printf(        "  +======================================+" ANSI_RESET "\n");
    }
    
    printf("\n");
}

void cli_print_about_screen(void) {
    cli_clear_screen();
    print_theme_colors();

    printf(ANSI_BOLD "\n=======================================\n");
    printf("              " ANSI_YELLOW " ABOUT " ANSI_RESET "\n");
    printf(ANSI_BOLD "=======================================\n\n");
    printf("  " ANSI_CYAN "Game:    " ANSI_RESET "%s\n", APP_NAME);
    printf("  " ANSI_CYAN "Version: " ANSI_RESET "v%s\n", APP_VERSION);
    printf("  " ANSI_CYAN "Author:  " ANSI_RESET "%s\n", APP_AUTHOR);
    printf("  " ANSI_CYAN "Contact: " ANSI_RESET "%s\n", APP_CONTACT);
    printf("  " ANSI_CYAN "Mode:    " ANSI_RESET "CLI, AI, LAN, Internet, Replay, Achievements\n");
    printf("  " ANSI_CYAN "Security:" ANSI_RESET " OpenSSL session encryption on network modes\n");
    printf("\n  " ANSI_CYAN "[Enter] Back to Main Menu" ANSI_RESET "\n\n");
    printf(ANSI_BOLD "=======================================\n");
}

void cli_print_ai_thinking(void) {
    printf("\r  " ANSI_YELLOW "AI is thinking" ANSI_RESET);
    fflush(stdout);
    
    for (int i = 0; i < 3; i++) {
        printf(".");
        fflush(stdout);
        delay_seconds(1);
    }
    
    printf("\r                    \r");
}

void cli_print_highscores(const Score* score) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n=======================================\n");
    printf(       "         " ANSI_YELLOW " HIGH SCORES " ANSI_RESET "\n");
    printf(ANSI_BOLD "=======================================\n\n");

    const int wins = score ? score->wins : 0;
    const int losses = score ? score->losses : 0;
    const int draws = score ? score->draws : 0;
    const int total_games = wins + losses + draws;
    const double win_rate = (total_games > 0) ? (100.0 * (double)wins / (double)total_games) : 0.0;

    printf("  " ANSI_CYAN "Total Results" ANSI_RESET "\n");
    printf("  " ANSI_WHITE "─────────────────────────\n");
    printf("  " ANSI_GREEN "Wins:   " ANSI_RESET "%d\n", wins);
    printf("  " ANSI_RED "Losses: " ANSI_RESET "%d\n", losses);
    printf("  " ANSI_YELLOW "Draws:  " ANSI_RESET "%d\n", draws);
    printf("  " ANSI_CYAN "Games:  " ANSI_RESET "%d\n", total_games);
    printf("  " ANSI_CYAN "Win Rate: " ANSI_RESET "%.1f%%\n", win_rate);

    printf("\n  " ANSI_CYAN "[Enter] Back to Main Menu" ANSI_RESET "\n\n");
    printf(ANSI_BOLD "=======================================\n");
}

void cli_print_settings_menu(const Config* cfg, int selected_index) {
    const int board_size = (cfg && cfg->board_size >= MIN_BOARD_SIZE && cfg->board_size <= MAX_BOARD_SIZE)
        ? cfg->board_size
        : 3;
    const int theme = (cfg && cfg->color_theme >= 0 && cfg->color_theme <= 3) ? cfg->color_theme : 0;
    const int timer_seconds = (cfg && cfg->timer_enabled && cfg->timer_seconds > 0) ? cfg->timer_seconds : 0;
    const char player_symbol = (cfg && (cfg->player_symbol == 'X' || cfg->player_symbol == 'O'))
        ? cfg->player_symbol
        : 'X';
    const bool sound_enabled = cfg ? cfg->sound_enabled : true;

    char option_board[64];
    char option_theme[64];
    char option_timer[64];
    char option_symbol[64];
    char option_sound[64];

    (void)snprintf(option_board, sizeof(option_board), "Board Size: %dx%d", board_size, board_size);
    (void)snprintf(option_theme, sizeof(option_theme), "Color Theme: %s", theme_name(theme));
    if (timer_seconds > 0) {
        (void)snprintf(option_timer, sizeof(option_timer), "Timer: %ds", timer_seconds);
    } else {
        (void)snprintf(option_timer, sizeof(option_timer), "Timer: Off");
    }
    (void)snprintf(option_symbol, sizeof(option_symbol), "Player Symbol: %c", player_symbol);
    (void)snprintf(option_sound, sizeof(option_sound), "Sound: %s", sound_enabled ? "On" : "Off");

    static const char* subtitles[] = {
        "3x3, 4x4, or 5x5",
        "Default, Dark, Light, Retro",
        "Seconds per move (0 = off)",
        "Choose X or O",
        "Toggle game audio",
        "Return to main menu"
    };
    const int option_count = 6;

    if (selected_index < 0 || selected_index >= option_count) {
        selected_index = 0;
    }

    const char* options[] = {
        option_board,
        option_theme,
        option_timer,
        option_symbol,
        option_sound,
        "Back"
    };

    render_live_menu("settings", "SETTINGS", options, subtitles, option_count, selected_index);

    const int config_line_row = menu_footer_row(option_count) + 3;
    printf("\033[%d;1H\033[2K" ANSI_GRAY "  Config: %s" ANSI_RESET, config_line_row, get_config_path());
    printf("\033[%d;1H\033[2K" ANSI_GRAY "  Edit this file with your preferred text editor." ANSI_RESET,
           config_line_row + 1);
    printf("\033[%d;1H", config_line_row + 2);
    fflush(stdout);
}

void cli_print_achievements_menu(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n=======================================\n");
    printf(       "      " ANSI_YELLOW " ACHIEVEMENTS " ANSI_RESET "\n");
    printf(ANSI_BOLD "=======================================\n\n");
    
    printf("  " ANSI_CYAN "[1]" ANSI_WHITE "  First Win           " ANSI_GREEN "✓\n");
    printf("  " ANSI_CYAN "[2]" ANSI_WHITE "  Win 5 Games         " ANSI_YELLOW "✓\n");
    printf("  " ANSI_CYAN "[3]" ANSI_WHITE "  Win 10 Games        " ANSI_RESET "\n");
    printf("  " ANSI_CYAN "[4]" ANSI_WHITE "  Beat Hard AI        " ANSI_RESET "\n");
    printf("  " ANSI_CYAN "[5]" ANSI_WHITE "  Draw Master         " ANSI_RESET "\n");
    printf("  " ANSI_CYAN "[6]" ANSI_WHITE "  Unstoppable (10 streak) " ANSI_RESET "\n\n");
    
    printf("  " ANSI_CYAN "[B]ack to Main Menu" ANSI_RESET "\n\n");
    printf(ANSI_BOLD "=======================================\n");
}

void cli_print_replay_menu(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n=======================================\n");
    printf(       "      " ANSI_YELLOW " REPLAY MODE " ANSI_RESET "\n");
    printf(ANSI_BOLD "=======================================\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[1]" ANSI_WHITE "  View Past Games\n\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[2]" ANSI_WHITE "  Watch Game Replay\n\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[B]" ANSI_CYAN "  Back to Main Menu\n\n");
    
    printf(ANSI_BOLD "=======================================\n");
    printf("  " ANSI_CYAN "> " ANSI_RESET);
}

void cli_print_game_controls(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n=======================================\n");
    printf(       "      " ANSI_YELLOW " GAME CONTROLS " ANSI_RESET "\n");
    printf(ANSI_BOLD "=======================================\n\n");
    
    printf(ANSI_WHITE "  Enter move: " ANSI_CYAN "23" ANSI_WHITE " or " ANSI_CYAN "2 3" ANSI_WHITE "\n\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[U]" ANSI_WHITE "  Undo last move\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[R]" ANSI_WHITE "  Redo move\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[Q]" ANSI_WHITE "  Quit to menu\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[S]" ANSI_WHITE "  Save replay\n\n");
    
    printf(ANSI_BOLD "=======================================\n");
}

void cli_print_timer(int seconds) {
    if (seconds <= 5) {
        printf(ANSI_RED "  [TIME: %ds] " ANSI_RESET, seconds);
    } else if (seconds <= 10) {
        printf(ANSI_YELLOW "  [TIME: %ds] " ANSI_RESET, seconds);
    } else {
        printf(ANSI_GREEN "  [TIME: %ds] " ANSI_RESET, seconds);
    }
}

void cli_print_undo_redo_status(Game* game) {
    if (!game) return;
    
    printf("  ");
    if (game_can_undo(game)) {
        printf(ANSI_CYAN "[U]Undo" ANSI_RESET);
    } else {
        printf(ANSI_GRAY "[U]Undo" ANSI_RESET);
    }
    
    printf("  ");
    
    if (game_can_redo(game)) {
        printf(ANSI_CYAN "[R]Redo" ANSI_RESET);
    } else {
        printf(ANSI_GRAY "[R]Redo" ANSI_RESET);
    }
    
    printf("\n");
}
