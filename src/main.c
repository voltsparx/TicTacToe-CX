#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
#endif

#include "cli.h"
#include "game.h"
#include "ai.h"
#include "network.h"
#include "utils.h"
#include "gui.h"
#include "sound.h"

static Config global_config;
static Score global_score;
static Sound global_sound;

static void get_input(char* buffer, size_t size);
static void play_local_2p(Game* game);
static void play_ai(Game* game, GameMode mode);
static void play_network(Network* net, bool is_host);
static void run_ai_turn(Game* game);
static void print_welcome_animation(void);
static void sleep_seconds(unsigned int seconds);
static bool parse_move_input(const char* input, uint8_t board_size, uint8_t* row, uint8_t* col);
static void apply_config_to_game(Game* game);
static bool launch_gui_mode(void);
static void play_game_end_sound(const Game* game);

int main(int argc, char* argv[]) {
    bool request_gui = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gui") == 0 || strcmp(argv[i], "-g") == 0) {
            request_gui = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [--gui|-g]\n", argv[0]);
            printf("  --gui, -g  Launch GUI mode when available\n");
            return 0;
        }
    }

    srand((unsigned int)time(NULL));
    
    config_load(&global_config, get_config_path());
    score_load(&global_score, get_highscore_path());
    sound_init(&global_sound);
    sound_set_enabled(&global_sound, global_config.sound_enabled);
    
    cli_set_theme((ColorTheme)global_config.color_theme);

    if (request_gui) {
        if (launch_gui_mode()) {
            sound_close(&global_sound);
            return 0;
        }
#ifdef __ANDROID__
        sound_close(&global_sound);
        return 1;
#else
        printf(ANSI_YELLOW "  Falling back to CLI mode.\n" ANSI_RESET);
        sleep_seconds(1);
#endif
    }
    
    print_welcome_animation();
    
    char input[32];
    
    while (1) {
        cli_print_main_menu();
        get_input(input, sizeof(input));
        
        char choice = toupper(input[0]);
        
        if (choice == 'Q') {
            sound_play(&global_sound, SOUND_MENU);
            printf(ANSI_YELLOW "\n  Thanks for playing TicTacToe-CX!\n");
            printf(ANSI_CYAN "  Made with " ANSI_RED "\u2665 " ANSI_CYAN "by voltsparx\n\n" ANSI_RESET);
            break;
        }
        
        switch (choice) {
            case '1': {
                sound_play(&global_sound, SOUND_MENU);
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
                    apply_config_to_game(&game);
                    play_ai(&game, mode);
                }
                break;
            }
            
            case '2': {
                sound_play(&global_sound, SOUND_MENU);
                Game game;
                game_init(&game, (uint8_t)global_config.board_size, MODE_LOCAL_2P);
                apply_config_to_game(&game);
                play_local_2p(&game);
                break;
            }
            
            case '3': {
                sound_play(&global_sound, SOUND_MENU);
                while (1) {
                    cli_print_network_menu();
                    get_input(input, sizeof(input));
                    
                    if (toupper(input[0]) == 'B') break;
                    
                    bool is_host = (input[0] == '1');
                    
                    if (input[0] == '1' || input[0] == '2') {
                        Network net;
                        if (!network_init(&net)) {
                            printf(ANSI_ERROR "\n  Network subsystem failed to initialize.\n" ANSI_RESET);
                            sound_play(&global_sound, SOUND_INVALID);
                            sleep_seconds(2);
                            continue;
                        }
                        play_network(&net, is_host);
                    }
                }
                break;
            }
            
            case '4': {
                sound_play(&global_sound, SOUND_MENU);
                cli_print_highscores(&global_score);
                get_input(input, sizeof(input));
                break;
            }
            
            case '5': {
                sound_play(&global_sound, SOUND_MENU);
                while (1) {
                    cli_print_settings_menu(&global_config);
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
                            sound_play(&global_sound, SOUND_MENU);
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
                            sound_play(&global_sound, SOUND_MENU);
                        }
                    } else if (input[0] == '3') {
                        printf(ANSI_CYAN "\n  Timer in seconds (0=off): " ANSI_RESET);
                        get_input(input, sizeof(input));
                        int timer = atoi(input);
                        if (timer < 0) {
                            timer = 0;
                        }
                        global_config.timer_seconds = timer;
                        global_config.timer_enabled = (timer > 0);
                        config_save(&global_config, get_config_path());
                        printf(ANSI_GREEN "  Timer set to %d seconds\n" ANSI_RESET, timer);
                        sound_play(&global_sound, SOUND_MENU);
                    } else if (input[0] == '4') {
                        printf(ANSI_CYAN "\n  Select player symbol (X/O): " ANSI_RESET);
                        get_input(input, sizeof(input));
                        char symbol = (char)toupper((unsigned char)input[0]);
                        if (symbol == 'X' || symbol == 'O') {
                            global_config.player_symbol = symbol;
                            config_save(&global_config, get_config_path());
                            printf(ANSI_GREEN "  Player symbol set to %c\n" ANSI_RESET, symbol);
                            sound_play(&global_sound, SOUND_MENU);
                        } else {
                            printf(ANSI_ERROR "  Invalid symbol! Use X or O.\n" ANSI_RESET);
                            sound_play(&global_sound, SOUND_INVALID);
                        }
                    } else if (input[0] == '5') {
                        global_config.sound_enabled = !global_config.sound_enabled;
                        sound_set_enabled(&global_sound, global_config.sound_enabled);
                        config_save(&global_config, get_config_path());
                        printf(ANSI_GREEN "  Sound %s\n" ANSI_RESET, global_config.sound_enabled ? "enabled" : "disabled");
                        if (global_config.sound_enabled) {
                            sound_play(&global_sound, SOUND_MENU);
                        }
                    }
                }
                break;
            }
            default:
                sound_play(&global_sound, SOUND_INVALID);
                break;
        }
    }
    
    score_save(&global_score, get_highscore_path());
    sound_close(&global_sound);
    
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
        sleep_seconds(1);
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
    
    sleep_seconds(2);
}

static void play_local_2p(Game* game) {
    char input[32];

    while (game->state == GAME_STATE_PLAYING) {
        cli_print_board(game);
        cli_print_move_prompt();
        
        get_input(input, sizeof(input));
        
        if (toupper(input[0]) == 'Q') {
            printf(ANSI_YELLOW "\n  Game aborted!\n" ANSI_RESET);
            sound_play(&global_sound, SOUND_MENU);
            return;
        }

        uint8_t row, col;
        if (!parse_move_input(input, game->size, &row, &col)) {
            printf(ANSI_ERROR "  Invalid input! Use format: row col\n" ANSI_RESET);
            sound_play(&global_sound, SOUND_INVALID);
            sleep_seconds(1);
            continue;
        }

        if (!game_make_move(game, row, col)) {
            printf(ANSI_ERROR "  Invalid move! Try again.\n" ANSI_RESET);
            sound_play(&global_sound, SOUND_INVALID);
            sleep_seconds(1);
        } else {
            sound_play(&global_sound, SOUND_MOVE);
        }
    }
    
    cli_print_board(game);
    cli_print_game_over(game);
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game_get_winner(game);
        score_update(&global_score, (winner == PLAYER_X) ? 1 : -1);
    } else if (game->state == GAME_STATE_DRAW) {
        score_update(&global_score, 0);
    }
    
    score_save(&global_score, get_highscore_path());
    play_game_end_sound(game);
    
    printf("  " ANSI_CYAN "[Enter] Continue  [Q] Quit to Menu" ANSI_RESET "\n");
    get_input(input, sizeof(input));
}

static void play_ai(Game* game, GameMode mode) {
    const char* mode_names[] = {"Easy", "Medium", "Hard"};
    int mode_idx = (mode == MODE_AI_EASY) ? 0 : (mode == MODE_AI_MEDIUM) ? 1 : 2;
    
    printf(ANSI_GREEN "\n  Starting " ANSI_YELLOW "%s" ANSI_GREEN " AI game!\n\n" ANSI_RESET, mode_names[mode_idx]);
    sleep_seconds(1);
    
    while (game->state == GAME_STATE_PLAYING) {
        cli_print_board(game);
        
        if (game->current_player != game->player_symbol) {
            run_ai_turn(game);
        } else {
            cli_print_move_prompt();
            
            char input[32];
            get_input(input, sizeof(input));
            
            if (toupper(input[0]) == 'Q') {
                printf(ANSI_YELLOW "\n  Game aborted!\n" ANSI_RESET);
                sound_play(&global_sound, SOUND_MENU);
                return;
            }

            uint8_t row, col;
            if (!parse_move_input(input, game->size, &row, &col)) {
                printf(ANSI_ERROR "  Invalid input! Use format: row col\n" ANSI_RESET);
                sound_play(&global_sound, SOUND_INVALID);
                sleep_seconds(1);
                continue;
            }

            if (!game_make_move(game, row, col)) {
                printf(ANSI_ERROR "  Invalid move! Try again.\n" ANSI_RESET);
                sound_play(&global_sound, SOUND_INVALID);
                sleep_seconds(1);
            } else {
                sound_play(&global_sound, SOUND_MOVE);
            }
        }
    }
    
    cli_print_board(game);
    cli_print_game_over(game);
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game_get_winner(game);
        bool player_won = (winner == game->player_symbol);
        score_update(&global_score, player_won ? 1 : -1);
    } else if (game->state == GAME_STATE_DRAW) {
        score_update(&global_score, 0);
    }
    
    score_save(&global_score, get_highscore_path());
    play_game_end_sound(game);
    
    char input[32];
    printf("  " ANSI_CYAN "[Enter] Continue  [Q] Quit to Menu" ANSI_RESET "\n");
    get_input(input, sizeof(input));
}

static void run_ai_turn(Game* game) {
    printf(ANSI_YELLOW "  AI is thinking" ANSI_RESET);
    fflush(stdout);
    
    for (int i = 0; i < 3; i++) {
        sleep_seconds(1);
        printf(".");
        fflush(stdout);
    }
    
    printf("\n");
    
    Move ai_move;
    ai_get_move(game, &ai_move);
    
    if (ai_move.row < game->size && ai_move.col < game->size) {
        game_make_move(game, ai_move.row, ai_move.col);
        sound_play(&global_sound, SOUND_MOVE);
    }
}

static void play_network(Network* net, bool is_host) {
    char input[64];
    char passphrase[NETWORK_PASSPHRASE_MAX];
    int port = DEFAULT_PORT;

    printf(ANSI_BRIGHT_CYAN "\n  Enter shared passphrase (blank = default): " ANSI_RESET);
    get_input(passphrase, sizeof(passphrase));
    network_set_passphrase(net, passphrase);
    
    if (is_host) {
        printf(ANSI_CYAN "\n  Enter port (default %d): " ANSI_RESET, port);
        get_input(input, sizeof(input));
        if (strlen(input) > 0) {
            port = atoi(input);
            if (port <= 0 || port > 65535) port = DEFAULT_PORT;
        }
        
        if (network_host(net, port)) {
            printf(ANSI_GREEN "\n  Hosting on port %d...\n" ANSI_RESET, port);
            printf(ANSI_YELLOW "  Waiting for opponent to connect...\n" ANSI_RESET);

            int wait_seconds = 0;
            while (!net->connected && wait_seconds < 120) {
                if (network_accept(net, 1000)) {
                    break;
                }
                printf(ANSI_CYAN "." ANSI_RESET);
                fflush(stdout);
                wait_seconds++;
            }
            printf("\n");

            if (!net->connected) {
                printf(ANSI_ERROR "  Timed out waiting for opponent.\n" ANSI_RESET);
                sound_play(&global_sound, SOUND_INVALID);
                network_close(net);
                return;
            }

            printf(ANSI_GREEN "  Player connected!\n" ANSI_RESET);
            sound_play(&global_sound, SOUND_MENU);
        } else {
            printf(ANSI_ERROR "  Failed to host game!\n" ANSI_RESET);
            sound_play(&global_sound, SOUND_INVALID);
            sleep_seconds(2);
            return;
        }
    } else {
        char ip[16] = "127.0.0.1";
        printf(ANSI_CYAN "\n  Enter host IP (default 127.0.0.1): " ANSI_RESET);
        get_input(input, sizeof(input));
        if (strlen(input) > 0) {
            strncpy(ip, input, 15);
            ip[15] = '\0';
        }
        
        printf(ANSI_CYAN "  Enter port (default %d): " ANSI_RESET, port);
        get_input(input, sizeof(input));
        if (strlen(input) > 0) {
            port = atoi(input);
            if (port <= 0 || port > 65535) port = DEFAULT_PORT;
        }
        
        if (network_connect(net, ip, port)) {
            printf(ANSI_GREEN "\n  Connected to %s:%d!\n" ANSI_RESET, ip, port);
            sound_play(&global_sound, SOUND_MENU);
        } else {
            printf(ANSI_ERROR "  Failed to connect!\n" ANSI_RESET);
            sound_play(&global_sound, SOUND_INVALID);
            sleep_seconds(2);
            return;
        }
    }

    if (!network_secure_handshake(net, 10000)) {
        printf(ANSI_ERROR "  Secure handshake failed. Check passphrase and try again.\n" ANSI_RESET);
        sound_play(&global_sound, SOUND_INVALID);
        network_close(net);
        sleep_seconds(2);
        return;
    }

    printf(ANSI_BRIGHT_GREEN ANSI_BOLD "  Secure channel established (encrypted + verified)\n" ANSI_RESET);
    sound_play(&global_sound, SOUND_ACHIEVEMENT);
    
    Game game;
    game_init(&game, (uint8_t)global_config.board_size, 
              is_host ? MODE_NETWORK_HOST : MODE_NETWORK_CLIENT);
    apply_config_to_game(&game);
    game.player_symbol = is_host ? PLAYER_X : PLAYER_O;
    
    printf(ANSI_GREEN "\n  Game starting! You are %c\n\n" ANSI_RESET, 
           game.player_symbol == PLAYER_X ? 'X' : 'O');
    sleep_seconds(1);
    
    while (game.state == GAME_STATE_PLAYING || game.state == GAME_STATE_WAITING) {
        cli_print_board(&game);
        
        if (game.current_player == game.player_symbol) {
            cli_print_move_prompt();
            get_input(input, sizeof(input));

            if (toupper((unsigned char)input[0]) == 'Q') {
                printf(ANSI_YELLOW "  Leaving network game.\n" ANSI_RESET);
                sound_play(&global_sound, SOUND_MENU);
                break;
            }

            uint8_t row, col;
            if (!parse_move_input(input, game.size, &row, &col)) {
                sound_play(&global_sound, SOUND_INVALID);
                continue;
            }
            
            if (game_make_move(&game, row, col)) {
                if (!network_send_move(net, row, col)) {
                    printf(ANSI_ERROR "  Failed to send move. Connection lost.\n" ANSI_RESET);
                    sound_play(&global_sound, SOUND_INVALID);
                    break;
                }
                sound_play(&global_sound, SOUND_MOVE);
            } else {
                sound_play(&global_sound, SOUND_INVALID);
            }
        } else {
            printf(ANSI_CYAN "  Waiting for opponent...\n" ANSI_RESET);
            
            uint8_t row, col;
            if (network_receive_move(net, &row, &col, 30000)) {
                game_make_move(&game, row, col);
                sound_play(&global_sound, SOUND_MOVE);
            } else if (!net->connected) {
                printf(ANSI_ERROR "  Opponent disconnected.\n" ANSI_RESET);
                sound_play(&global_sound, SOUND_INVALID);
                break;
            }
        }
    }
    
    cli_print_board(&game);
    cli_print_game_over(&game);
    play_game_end_sound(&game);
    
    network_close(net);
    
    printf("  " ANSI_CYAN "[Enter] Continue" ANSI_RESET "\n");
    get_input(input, sizeof(input));
}

static bool launch_gui_mode(void) {
#ifdef __ANDROID__
    printf(ANSI_ERROR "\n  GUI mode is not supported on Termux/Android.\n" ANSI_RESET);
    return false;
#else
    GUIState gui;
    if (!gui_init(&gui) || !gui.use_gui) {
        printf(ANSI_ERROR "\n  GUI mode is unavailable (SDL2/SDL2_ttf missing).\n" ANSI_RESET);
        return false;
    }

    Game game;
    game_init(&game, (uint8_t)global_config.board_size, MODE_LOCAL_2P);
    apply_config_to_game(&game);

    printf(ANSI_BRIGHT_GREEN "\n  Launching GUI mode...\n" ANSI_RESET);
    (void)gui_run_game(&game);
    gui_close(&gui);
    return true;
#endif
}

static void play_game_end_sound(const Game* game) {
    if (!game) return;

    if (game->state == GAME_STATE_DRAW) {
        sound_play(&global_sound, SOUND_DRAW);
        return;
    }

    if (game->state != GAME_STATE_WIN) {
        return;
    }

    if (game->mode == MODE_LOCAL_2P) {
        sound_play(&global_sound, SOUND_WIN);
        return;
    }

    Player winner = game_get_winner(game);
    if (winner == game->player_symbol) {
        sound_play(&global_sound, SOUND_WIN);
    } else {
        sound_play(&global_sound, SOUND_LOSE);
    }
}

static void sleep_seconds(unsigned int seconds) {
#ifdef _WIN32
    Sleep(seconds * 1000U);
#else
    sleep(seconds);
#endif
}

static bool parse_move_input(const char* input, uint8_t board_size, uint8_t* row, uint8_t* col) {
    if (!input || !row || !col) {
        return false;
    }

    int user_row = 0;
    int user_col = 0;
    if (sscanf(input, "%d %d", &user_row, &user_col) != 2) {
        return false;
    }

    if (user_row < 1 || user_col < 1 || user_row > board_size || user_col > board_size) {
        return false;
    }

    *row = (uint8_t)(user_row - 1);
    *col = (uint8_t)(user_col - 1);
    return true;
}

static void apply_config_to_game(Game* game) {
    if (!game) {
        return;
    }

    if (global_config.timer_enabled && global_config.timer_seconds > 0) {
        game_start_timer(game, global_config.timer_seconds);
    } else {
        game_start_timer(game, 0);
    }

    if (global_config.player_symbol == 'O') {
        game->player_symbol = PLAYER_O;
    } else {
        game->player_symbol = PLAYER_X;
    }
}
