#ifndef CLI_H
#define CLI_H

#include <stdio.h>
#include "game.h"

#define ANSI_RESET      "\033[0m"
#define ANSI_BOLD       "\033[1m"
#define ANSI_UNDERLINE  "\033[4m"

#define ANSI_BLACK      "\033[30m"
#define ANSI_RED        "\033[31m"
#define ANSI_GREEN      "\033[32m"
#define ANSI_YELLOW     "\033[33m"
#define ANSI_BLUE       "\033[34m"
#define ANSI_MAGENTA    "\033[35m"
#define ANSI_CYAN       "\033[36m"
#define ANSI_WHITE      "\033[37m"
#define ANSI_GRAY       "\033[90m"

#define ANSI_BG_BLACK   "\033[40m"
#define ANSI_BG_RED     "\033[41m"
#define ANSI_BG_GREEN   "\033[42m"
#define ANSI_BG_YELLOW  "\033[43m"
#define ANSI_BG_BLUE    "\033[44m"
#define ANSI_BG_MAGENTA "\033[45m"
#define ANSI_BG_CYAN    "\033[46m"
#define ANSI_BG_WHITE   "\033[47m"

#define COLOR_X ANSI_CYAN ANSI_BOLD
#define COLOR_O ANSI_MAGENTA ANSI_BOLD
#define COLOR_WIN ANSI_GREEN ANSI_BOLD
#define COLOR_DRAW ANSI_YELLOW ANSI_BOLD
#define COLOR_PROMPT ANSI_CYAN
#define COLOR_ERROR ANSI_RED
#define ANSI_ERROR ANSI_RED
#define COLOR_MENU ANSI_YELLOW
#define COLOR_TITLE ANSI_CYAN ANSI_BOLD

typedef enum {
    THEME_DEFAULT,
    THEME_DARK,
    THEME_LIGHT,
    THEME_RETRO
} ColorTheme;

void cli_clear_screen(void);
void cli_print_title(void);
void cli_print_main_menu(void);
void cli_print_game_menu(void);
void cli_print_network_menu(void);
void cli_print_board(Game* game);
void cli_print_move_prompt(Game* game);
void cli_print_game_over(Game* game);
void cli_print_ai_thinking(void);
void cli_set_theme(ColorTheme theme);
void cli_print_highscores(void);
void cli_print_settings_menu(void);
void cli_print_achievements_menu(void);
void cli_print_replay_menu(void);
void cli_print_game_controls(void);
void cli_print_timer(int seconds);
void cli_print_undo_redo_status(Game* game);

#endif
