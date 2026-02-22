#include "cli.h"
#include <string.h>
#include <time.h>
#include <unistd.h>

static ColorTheme current_theme = THEME_DEFAULT;

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
    printf(ANSI_WHITE "        └── Board size, theme, timer\n\n");
    
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
            
            bool is_winning_cell = false;
            if (game->state == GAME_STATE_WIN && game->win_line[0] != -1) {
                if ((game->win_line[0] == i && game->win_line[1] == j) ||
                    (game->win_line[2] == i && game->win_line[3] == j)) {
                    is_winning_cell = true;
                }
            }
            
            if (c == 'X') {
                printf("│" COLOR_X "   X   " ANSI_RESET);
            } else if (c == 'O') {
                printf("│" COLOR_O "   O   " ANSI_RESET);
            } else if (is_winning_cell) {
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
        printf("  " COLOR_X "▶ X's Turn" ANSI_RESET);
    } else {
        printf("  " COLOR_O "▶ O's Turn" ANSI_RESET);
    }
    
    printf("\n");
}

void cli_print_move_prompt(Game* game) {
    print_theme_colors();
    printf("\n  " COLOR_PROMPT "Enter move (row col): " ANSI_RESET);
}

void cli_print_game_over(Game* game) {
    printf("\n");
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game->current_player == PLAYER_X ? PLAYER_O : PLAYER_X;
        if (winner == PLAYER_X) {
            printf(ANSI_BOLD "  ╔═══════════════════════════════════════╗\n");
            printf(        "  ║      " COLOR_X "  X WINS!  " ANSI_BOLD "               ║\n");
            printf(        "  ╚═══════════════════════════════════════╝" ANSI_RESET "\n");
        } else {
            printf(ANSI_BOLD "  ╔═══════════════════════════════════════╗\n");
            printf(        "  ║      " COLOR_O "  O WINS!  " ANSI_BOLD "               ║\n");
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
        sleep(1);
    }
    
    printf("\r                    \r");
}

void cli_print_highscores(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "         " ANSI_YELLOW " HIGH SCORES " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf("  " ANSI_CYAN "Mode" ANSI_RESET "          " ANSI_CYAN "W" ANSI_RESET "  " ANSI_CYAN "L" ANSI_RESET "  " ANSI_CYAN "D" ANSI_RESET "\n");
    printf("  " ANSI_WHITE "─────────────────────────\n");
    printf("  " ANSI_WHITE "vs Easy AI     " ANSI_RESET "   -   -   -\n");
    printf("  " ANSI_WHITE "vs Medium AI    " ANSI_RESET "   -   -   -\n");
    printf("  " ANSI_WHITE "vs Hard AI      " ANSI_RESET "   -   -   -\n");
    printf("  " ANSI_WHITE "Two Player      " ANSI_RESET "   -   -   -\n");
    printf("  " ANSI_WHITE "LAN Multiplayer " ANSI_RESET "   -   -   -\n");
    
    printf("\n  " ANSI_CYAN "[B]ack to Main Menu" ANSI_RESET "\n\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
}

void cli_print_settings_menu(void) {
    print_theme_colors();
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "         " ANSI_YELLOW " SETTINGS " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "1" ANSI_CYAN "]" ANSI_WHITE "  Board Size:     " ANSI_YELLOW "3x3" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── 3x3, 4x4, or 5x5\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "2" ANSI_CYAN "]" ANSI_WHITE "  Color Theme:    " ANSI_YELLOW "Default" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Dark / Light / Retro\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "3" ANSI_CYAN "]" ANSI_WHITE "  Timer:          " ANSI_YELLOW "Off" ANSI_RESET "\n");
    printf(ANSI_WHITE "        └── Seconds per move\n\n");
    
    printf(ANSI_WHITE "  " ANSI_CYAN "[" ANSI_RESET "4" ANSI_CYAN "]" ANSI_WHITE "  Custom Symbols: " ANSI_YELLOW "X/O" ANSI_RESET "\n\n");
    
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
