#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "cli.h"
#include "game.h"
#include "ai.h"
#include "network.h"
#include "utils.h"

static Config global_config;
static Score global_score;

static void get_input(char* buffer, size_t size);
static void play_local_2p(Game* game);
static void play_ai(Game* game, GameMode mode);
static void play_network(Network* net, bool is_host);
static void run_ai_turn(Game* game);
static void print_welcome_animation(void);

int main(int argc, char* argv[]) {
    srand((unsigned int)time(NULL));
    
    config_load(&global_config, get_config_path());
    score_load(&global_score, get_highscore_path());
    
    cli_set_theme((ColorTheme)global_config.color_theme);
    
    print_welcome_animation();
    
    char input[32];
    
    while (1) {
        cli_print_main_menu();
        get_input(input, sizeof(input));
        
        char choice = toupper(input[0]);
        
        if (choice == 'Q') {
            printf(ANSI_YELLOW "\n  Thanks for playing TicTacToe-CX!\n");
            printf(ANSI_CYAN "  Made with " ANSI_RED "\u2665 " ANSI_CYAN "by voltsparx\n\n" ANSI_RESET);
            break;
        }
        
        switch (choice) {
            case '1': {
                while (1) {
                    cli_print_game_menu();
                    get_input(input, sizeof(input));
                    
                    if (toupper(input[0]) == 'B') break;
                    
                    GameMode mode = MODE_AI_EASY;
                    if (input[0] == '1') mode = MODE_AI_EASY;
                    else if (input[0] == '2') mode = MODE_AI_MEDIUM;
                    else if (input[0] == '3') mode = MODE_AI_HARD;
                    else continue;
                    
                    Game game;
                    game_init(&game, (uint8_t)global_config.board_size, mode);
                    play_ai(&game, mode);
                }
                break;
            }
            
            case '2': {
                Game game;
                game_init(&game, (uint8_t)global_config.board_size, MODE_LOCAL_2P);
                play_local_2p(&game);
                break;
            }
            
            case '3': {
                while (1) {
                    cli_print_network_menu();
                    get_input(input, sizeof(input));
                    
                    if (toupper(input[0]) == 'B') break;
                    
                    bool is_host = (input[0] == '1');
                    
                    if (input[0] == '1' || input[0] == '2') {
                        Network net;
                        network_init(&net);
                        play_network(&net, is_host);
                    }
                }
                break;
            }
            
            case '4': {
                cli_print_highscores();
                get_input(input, sizeof(input));
                break;
            }
            
            case '5': {
                while (1) {
                    cli_print_settings_menu();
                    get_input(input, sizeof(input));
                    
                    if (toupper(input[0]) == 'B') break;
                    
                    if (input[0] == '1') {
                        printf(ANSI_CYAN "\n  Enter board size (3-5): " ANSI_RESET);
                        get_input(input, sizeof(input));
                        int size = atoi(input);
                        if (size >= 3 && size <= 5) {
                            global_config.board_size = size;
                            config_save(&global_config, get_config_path());
                            printf(ANSI_GREEN "  Board size set to %dx%d\n" ANSI_RESET, size, size);
                        }
                    } else if (input[0] == '2') {
                        printf(ANSI_CYAN "\n  Select theme (0=Default, 1=Dark, 2=Light, 3=Retro): " ANSI_RESET);
                        get_input(input, sizeof(input));
                        int theme = atoi(input);
                        if (theme >= 0 && theme <= 3) {
                            global_config.color_theme = theme;
                            cli_set_theme((ColorTheme)theme);
                            config_save(&global_config, get_config_path());
                            printf(ANSI_GREEN "  Theme updated!\n" ANSI_RESET);
                        }
                    } else if (input[0] == '3') {
                        printf(ANSI_CYAN "\n  Timer in seconds (0=off): " ANSI_RESET);
                        get_input(input, sizeof(input));
                        int timer = atoi(input);
                        global_config.timer_seconds = timer;
                        global_config.timer_enabled = (timer > 0);
                        config_save(&global_config, get_config_path());
                        printf(ANSI_GREEN "  Timer set to %d seconds\n" ANSI_RESET, timer);
                    }
                }
                break;
            }
        }
    }
    
    score_save(&global_score, get_highscore_path());
    
    return 0;
}

static void get_input(char* buffer, size_t size) {
    if (!fgets(buffer, (int)size, stdin)) {
        buffer[0] = '\0';
        return;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
}

static void print_welcome_animation(void) {
    cli_print_title();
    
    printf(ANSI_YELLOW "\n  Loading");
    fflush(stdout);
    for (int i = 0; i < 3; i++) {
        sleep(1);
        printf(".");
        fflush(stdout);
    }
    printf("\n\n" ANSI_RESET);
    
    printf(ANSI_CYAN "  Welcome to " ANSI_YELLOW "TicTacToe-CX" ANSI_CYAN "!\n\n");
    printf(ANSI_WHITE "  Features:\n");
    printf(ANSI_GREEN "   * " ANSI_RESET "Single Player vs AI (Easy/Medium/Hard)\n");
    printf(ANSI_GREEN "   * " ANSI_RESET "Two Player Local Mode\n");
    printf(ANSI_GREEN "   * " ANSI_RESET "LAN Multiplayer\n");
    printf(ANSI_GREEN "   * " ANSI_RESET "Beautiful ANSI Color Themes\n");
    printf(ANSI_GREEN "   * " ANSI_RESET "Multiple Board Sizes\n\n");
    
    sleep(2);
}

static void play_local_2p(Game* game) {
    while (game->state == GAME_STATE_PLAYING) {
        cli_print_board(game);
        cli_print_move_prompt(game);
        
        char input[32];
        get_input(input, sizeof(input));
        
        if (toupper(input[0]) == 'Q') {
            printf(ANSI_YELLOW "\n  Game aborted!\n" ANSI_RESET);
            return;
        }
        
        int row, col;
        if (sscanf(input, "%d %d", &row, &col) != 2) {
            printf(ANSI_ERROR "  Invalid input! Use format: row col\n" ANSI_RESET);
            sleep(1);
            continue;
        }
        
        if (!game_make_move(game, (uint8_t)(row - 1), (uint8_t)(col - 1))) {
            printf(ANSI_ERROR "  Invalid move! Try again.\n" ANSI_RESET);
            sleep(1);
        }
    }
    
    cli_print_board(game);
    cli_print_game_over(game);
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game->current_player == PLAYER_X ? PLAYER_O : PLAYER_X;
        score_update(&global_score, (winner == PLAYER_X) ? 1 : -1);
    } else if (game->state == GAME_STATE_DRAW) {
        score_update(&global_score, 0);
    }
    
    score_save(&global_score, get_highscore_path());
    
    printf("  " ANSI_CYAN "[Enter] Continue  [Q] Quit to Menu" ANSI_RESET "\n");
    get_input(input, sizeof(input));
}

static void play_ai(Game* game, GameMode mode) {
    const char* mode_names[] = {"Easy", "Medium", "Hard"};
    int mode_idx = (mode == MODE_AI_EASY) ? 0 : (mode == MODE_AI_MEDIUM) ? 1 : 2;
    
    printf(ANSI_GREEN "\n  Starting " ANSI_YELLOW "%s" ANSI_GREEN " AI game!\n\n" ANSI_RESET, mode_names[mode_idx]);
    sleep(1);
    
    while (game->state == GAME_STATE_PLAYING) {
        cli_print_board(game);
        
        if (game->current_player != game->player_symbol) {
            run_ai_turn(game);
        } else {
            cli_print_move_prompt(game);
            
            char input[32];
            get_input(input, sizeof(input));
            
            if (toupper(input[0]) == 'Q') {
                printf(ANSI_YELLOW "\n  Game aborted!\n" ANSI_RESET);
                return;
            }
            
            int row, col;
            if (sscanf(input, "%d %d", &row, &col) != 2) {
                printf(ANSI_ERROR "  Invalid input! Use format: row col\n" ANSI_RESET);
                sleep(1);
                continue;
            }
            
            if (!game_make_move(game, (uint8_t)(row - 1), (uint8_t)(col - 1))) {
                printf(ANSI_ERROR "  Invalid move! Try again.\n" ANSI_RESET);
                sleep(1);
            }
        }
    }
    
    cli_print_board(game);
    cli_print_game_over(game);
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game->current_player == PLAYER_X ? PLAYER_O : PLAYER_X;
        bool player_won = (winner == game->player_symbol);
        score_update(&global_score, player_won ? 1 : -1);
    } else if (game->state == GAME_STATE_DRAW) {
        score_update(&global_score, 0);
    }
    
    score_save(&global_score, get_highscore_path());
    
    char input[32];
    printf("  " ANSI_CYAN "[Enter] Continue  [Q] Quit to Menu" ANSI_RESET "\n");
    get_input(input, sizeof(input));
}

static void run_ai_turn(Game* game) {
    printf(ANSI_YELLOW "  AI is thinking" ANSI_RESET);
    fflush(stdout);
    
    for (int i = 0; i < 3; i++) {
        sleep(1);
        printf(".");
        fflush(stdout);
    }
    
    printf("\n");
    
    Move ai_move;
    ai_get_move(game, &ai_move);
    
    if (ai_move.row < game->size && ai_move.col < game->size) {
        game_make_move(game, ai_move.row, ai_move.col);
    }
}

static void play_network(Network* net, bool is_host) {
    char input[64];
    int port = DEFAULT_PORT;
    
    if (is_host) {
        printf(ANSI_CYAN "\n  Enter port (default %d): " ANSI_RESET, port);
        get_input(input, sizeof(input));
        if (strlen(input) > 0) {
            port = atoi(input);
            if (port <= 0) port = DEFAULT_PORT;
        }
        
        if (network_host(net, port)) {
            printf(ANSI_GREEN "\n  Hosting on port %d...\n" ANSI_RESET, port);
            printf(ANSI_YELLOW "  Waiting for opponent to connect...\n" ANSI_RESET);
            
            while (!net->connected) {
                sleep(1);
            }
            printf(ANSI_GREEN "  Player connected!\n" ANSI_RESET);
        } else {
            printf(ANSI_ERROR "  Failed to host game!\n" ANSI_RESET);
            sleep(2);
            return;
        }
    } else {
        char ip[16] = "127.0.0.1";
        printf(ANSI_CYAN "\n  Enter host IP (default 127.0.0.1): " ANSI_RESET);
        get_input(input, sizeof(input));
        if (strlen(input) > 0) {
            strncpy(ip, input, 15);
        }
        
        printf(ANSI_CYAN "  Enter port (default %d): " ANSI_RESET, port);
        get_input(input, sizeof(input));
        if (strlen(input) > 0) {
            port = atoi(input);
            if (port <= 0) port = DEFAULT_PORT;
        }
        
        if (network_connect(net, ip, port)) {
            printf(ANSI_GREEN "\n  Connected to %s:%d!\n" ANSI_RESET, ip, port);
        } else {
            printf(ANSI_ERROR "  Failed to connect!\n" ANSI_RESET);
            sleep(2);
            return;
        }
    }
    
    Game game;
    game_init(&game, (uint8_t)global_config.board_size, 
              is_host ? MODE_NETWORK_HOST : MODE_NETWORK_CLIENT);
    game.player_symbol = is_host ? PLAYER_X : PLAYER_O;
    
    printf(ANSI_GREEN "\n  Game starting! You are %c\n\n" ANSI_RESET, 
           game.player_symbol == PLAYER_X ? 'X' : 'O');
    sleep(2);
    
    while (game.state == GAME_STATE_PLAYING || game.state == GAME_STATE_WAITING) {
        cli_print_board(&game);
        
        if (game.current_player == game.player_symbol) {
            cli_print_move_prompt(&game);
            get_input(input, sizeof(input));
            
            int row, col;
            if (sscanf(input, "%d %d", &row, &col) != 2) {
                continue;
            }
            
            if (game_make_move(&game, (uint8_t)(row - 1), (uint8_t)(col - 1))) {
                network_send_move(net, (uint8_t)(row - 1), (uint8_t)(col - 1));
            }
        } else {
            printf(ANSI_CYAN "  Waiting for opponent...\n" ANSI_RESET);
            
            uint8_t row, col;
            if (network_receive_move(net, &row, &col, 30000)) {
                game_make_move(&game, row, col);
            }
        }
    }
    
    cli_print_board(&game);
    cli_print_game_over(game);
    
    network_close(net);
    
    printf("  " ANSI_CYAN "[Enter] Continue" ANSI_RESET "\n");
    get_input(input, sizeof(input));
}
