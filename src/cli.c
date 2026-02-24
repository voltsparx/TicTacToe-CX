#include "cli.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

static ColorTheme current_theme = THEME_DEFAULT;

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
    
    printf(ANSI_CYAN ANSI_BOLD "\n  TicTacToe-CX " ANSI_YELLOW "v1.0\n\n" ANSI_RESET);
}

void cli_print_main_menu(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
    printf(       "           " ANSI_YELLOW " MAIN MENU " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "1" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_YELLOW "Play vs AI" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Easy / Medium / Hard\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "2" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_YELLOW "Two Player (Local)" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Play with a friend\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "3" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_YELLOW "LAN Multiplayer" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Host or Join a game\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "4" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_YELLOW "High Scores" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── View your achievements\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "5" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_YELLOW "Settings" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Board size, theme, timer, sound\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "Q" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_RED "Quit" ANSI_RESET "\n\n");
    
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
    printf("  " ANSI_CYAN "> " ANSI_RESET);
}

void cli_print_game_menu(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "         " ANSI_YELLOW " GAME MODE " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "1" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_GREEN "Easy AI" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Random moves - Great for beginners\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "2" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_YELLOW "Medium AI" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Blocks wins & tries to win\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "3" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_RED "Hard AI" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Minimax algorithm - Unbeatable!\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "B" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_CYAN "Back to Main Menu" ANSI_RESET "\n\n");
    
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
    printf("  " ANSI_CYAN "> " ANSI_RESET);
}

void cli_print_network_menu(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "      " ANSI_YELLOW " LAN MULTIPLAYER " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "1" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_GREEN "Host Game" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Create a new game server\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "2" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_YELLOW "Join Game" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Connect to existing server\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "B" ANSI_CYAN "]" ANSI_WHITE "  " ANSI_CYAN "Back to Main Menu" ANSI_RESET "\n\n");
    
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
    printf("  " ANSI_CYAN "> " ANSI_RESET);
}

void cli_print_board(Game* game) {
    printf("\n");
    print_theme_colors();
    
    uint8_t n = game->size;
    uint8_t cell_width = 7;
    
    for (uint8_t i = 0; i < n; i++) {
        printf("   ");
        for (uint8_t j = 0; j < n; j++) {
            printf("┌");
            for (int k = 0; k < cell_width - 1; k++) printf("─");
        }
        printf("┐\n");
        
        printf(" %d ", i + 1);
        for (uint8_t j = 0; j < n; j++) {
            char c = game_get_cell_char(game, i, j);

            if (cell_is_on_win_line(game, i, j) && c != ' ') {
                printf("│" COLOR_WIN "   %c   " ANSI_RESET, c);
            } else if (c == game->symbol_x) {
                printf("│" COLOR_X "   %c   " ANSI_RESET, c);
            } else if (c == game->symbol_o) {
                printf("│" COLOR_O "   %c   " ANSI_RESET, c);
            } else if (cell_is_on_win_line(game, i, j)) {
                printf("│" COLOR_WIN "   •   " ANSI_RESET);
            } else {
                printf("│       ");
            }
        }
        printf("│\n");
        
        if (i == n - 1) {
            printf("   ");
            for (uint8_t j = 0; j < n; j++) {
                printf("└");
                for (int k = 0; k < cell_width - 1; k++) printf("─");
            }
            printf("┘\n");
        } else {
            printf("   ");
            for (uint8_t j = 0; j < n; j++) {
                printf("├");
                for (int k = 0; k < cell_width - 1; k++) printf("─");
            }
            printf("┤\n");
        }
    }
    
    printf("\n     ");
    for (uint8_t j = 0; j < n; j++) {
        printf("  %d   ", j + 1);
    }
    printf("\n\n");
    
    Player current = game->current_player;
    if (current == PLAYER_X) {
        printf("  " COLOR_X "▶ %c's Turn" ANSI_RESET, game->symbol_x);
    } else {
        printf("  " COLOR_O "▶ %c's Turn" ANSI_RESET, game->symbol_o);
    }
    
    printf("\n");
}

void cli_print_move_prompt(void) {
    print_theme_colors();
    printf("\n  " COLOR_PROMPT "Enter move (row col): " ANSI_RESET);
}

void cli_print_game_over(Game* game) {
    printf("\n");
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game_get_winner(game);
        if (winner == PLAYER_X) {
            printf(ANSI_BOLD "  ╔═══════════════════════════════════════╗\n");
            printf(        "  ║      " COLOR_X "  %c WINS!  " ANSI_BOLD "               ║\n", game->symbol_x);
            printf(        "  ╚═══════════════════════════════════════╝" ANSI_RESET "\n");
        } else {
            printf(ANSI_BOLD "  ╔═══════════════════════════════════════╗\n");
            printf(        "  ║      " COLOR_O "  %c WINS!  " ANSI_BOLD "               ║\n", game->symbol_o);
            printf(        "  ╚═══════════════════════════════════════╝" ANSI_RESET "\n");
        }
    } else if (game->state == GAME_STATE_DRAW) {
        printf(ANSI_BOLD "  ╔═══════════════════════════════════════╗\n");
        printf(        "  ║      " COLOR_DRAW "  IT'S A DRAW!  " ANSI_BOLD "           ║\n");
        printf(        "  ╚═══════════════════════════════════════╝" ANSI_RESET "\n");
    }
    
    printf("\n");
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
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "         " ANSI_YELLOW " HIGH SCORES " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");

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
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
}

void cli_print_settings_menu(const Config* cfg) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "         " ANSI_YELLOW " SETTINGS " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");

    const int board_size = (cfg && cfg->board_size >= MIN_BOARD_SIZE && cfg->board_size <= MAX_BOARD_SIZE)
        ? cfg->board_size
        : 3;
    const int theme = (cfg && cfg->color_theme >= 0 && cfg->color_theme <= 3) ? cfg->color_theme : 0;
    const int timer_seconds = (cfg && cfg->timer_enabled && cfg->timer_seconds > 0) ? cfg->timer_seconds : 0;
    const char player_symbol = (cfg && (cfg->player_symbol == 'X' || cfg->player_symbol == 'O'))
        ? cfg->player_symbol
        : 'X';
    const bool sound_enabled = cfg ? cfg->sound_enabled : true;

    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "1" ANSI_CYAN "]" ANSI_WHITE "  Board Size:     " ANSI_YELLOW "%dx%d" ANSI_RESET "\n",
           board_size, board_size);
    printf(ANSI_WHITE "        └── 3x3, 4x4, or 5x5\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "2" ANSI_CYAN "]" ANSI_WHITE "  Color Theme:    " ANSI_YELLOW "%s" ANSI_RESET "\n",
           theme_name(theme));
    printf(ANSI_WHITE "        └── Dark / Light / Retro\n\n");
    
    if (timer_seconds > 0) {
        printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "3" ANSI_CYAN "]" ANSI_WHITE "  Timer:          " ANSI_YELLOW "%ds" ANSI_RESET "\n",
               timer_seconds);
    } else {
        printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "3" ANSI_CYAN "]" ANSI_WHITE "  Timer:          " ANSI_YELLOW "Off" ANSI_RESET "\n");
    }
    printf(ANSI_WHITE "        └── Seconds per move\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "4" ANSI_CYAN "]" ANSI_WHITE "  Player Symbol:  " ANSI_YELLOW "%c" ANSI_RESET "\n\n",
           player_symbol);

    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "5" ANSI_CYAN "]" ANSI_WHITE "  Sound:          " ANSI_YELLOW "%s" ANSI_RESET "\n",
           sound_enabled ? "On" : "Off");
    printf(ANSI_WHITE "        └── Toggle game sound effects\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "B" ANSI_CYAN "]" ANSI_WHITE "  Back to Main Menu" ANSI_RESET "\n\n");
    
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
    printf("  " ANSI_CYAN "> " ANSI_RESET);
}

void cli_print_achievements_menu(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "      " ANSI_YELLOW " ACHIEVEMENTS " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf("  " ANSI_CYAN "[1]" ANSI_WHITE "  First Win           " ANSI_GREEN "✓\n");
    printf("  " ANSI_CYAN "[2]" ANSI_WHITE "  Win 5 Games         " ANSI_YELLOW "✓\n");
    printf("  " ANSI_CYAN "[3]" ANSI_WHITE "  Win 10 Games        " ANSI_RESET "\n");
    printf("  " ANSI_CYAN "[4]" ANSI_WHITE "  Beat Hard AI        " ANSI_RESET "\n");
    printf("  " ANSI_CYAN "[5]" ANSI_WHITE "  Draw Master         " ANSI_RESET "\n");
    printf("  " ANSI_CYAN "[6]" ANSI_WHITE "  Unstoppable (10 streak) " ANSI_RESET "\n\n");
    
    printf("  " ANSI_CYAN "[B]ack to Main Menu" ANSI_RESET "\n\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
}

void cli_print_replay_menu(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "      " ANSI_YELLOW " REPLAY MODE " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[1]" ANSI_WHITE "  View Past Games\n\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[2]" ANSI_WHITE "  Watch Game Replay\n\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[B]" ANSI_CYAN "  Back to Main Menu\n\n");
    
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
    printf("  " ANSI_CYAN "> " ANSI_RESET);
}

void cli_print_game_controls(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "      " ANSI_YELLOW " GAME CONTROLS " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf(ANSI_WHITE "  Enter move: " ANSI_CYAN "row col" ANSI_WHITE " (e.g., 1 1)\n\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[U]" ANSI_WHITE "  Undo last move\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[R]" ANSI_WHITE "  Redo move\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[Q]" ANSI_WHITE "  Quit to menu\n");
    printf(ANSI_WHITE "  " ANSI_CYAN "[S]" ANSI_WHITE "  Save replay\n\n");
    
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
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
