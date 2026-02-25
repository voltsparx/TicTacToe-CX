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
#include "app_meta.h"

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
static void sleep_milliseconds(unsigned int milliseconds);
static bool parse_move_input(const char* input, uint8_t board_size, uint8_t* row, uint8_t* col);
static void apply_config_to_game(Game* game);
static bool launch_gui_mode(void);
static void play_game_end_sound(const Game* game);
static int run_vertical_menu(void (*render_menu)(int), int option_count, int initial_index);
static void print_cli_usage(const char* program_name);
static void print_version(void);

int main(int argc, char* argv[]) {
    bool request_gui = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--gui") == 0 || strcmp(argv[i], "-g") == 0) {
            request_gui = true;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_cli_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            print_cli_usage(argv[0]);
            return 1;
        }
    }

    cli_init_terminal();
    srand((unsigned int)time(NULL));

    if (!init_data_paths(true)) {
        fprintf(stderr, ANSI_ERROR "\n  Failed to initialize data directory.\n" ANSI_RESET);
        return 1;
    }
    
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
    int main_selection = 0;

    while (1) {
        int choice = run_vertical_menu(cli_print_main_menu, 7, main_selection);
        if (choice < 0) {
            choice = 6;
        }
        main_selection = choice;
        sound_play(&global_sound, SOUND_MENU);

        if (choice == 6) {
            printf(ANSI_YELLOW "\n  Thanks for playing TicTacToe-CX!\n");
            printf(ANSI_CYAN "  Made with " ANSI_RED "\u2665 " ANSI_CYAN "by voltsparx\n\n" ANSI_RESET);
            break;
        }

        switch (choice) {
            case 0: {
                int ai_selection = 0;
                while (1) {
                    int ai_choice = run_vertical_menu(cli_print_game_menu, 4, ai_selection);
                    if (ai_choice < 0 || ai_choice == 3) {
                        break;
                    }

                    ai_selection = ai_choice;
                    GameMode mode = (ai_choice == 0) ? MODE_AI_EASY
                                  : (ai_choice == 1) ? MODE_AI_MEDIUM
                                                     : MODE_AI_HARD;

                    Game game;
                    game_init(&game, (uint8_t)global_config.board_size, mode);
                    apply_config_to_game(&game);
                    play_ai(&game, mode);
                }
                break;
            }

            case 1: {
                Game game;
                game_init(&game, (uint8_t)global_config.board_size, MODE_LOCAL_2P);
                apply_config_to_game(&game);
                play_local_2p(&game);
                break;
            }

            case 2: {
                int network_selection = 0;
                while (1) {
                    int net_choice = run_vertical_menu(cli_print_network_menu, 3, network_selection);
                    if (net_choice < 0 || net_choice == 2) {
                        break;
                    }

                    network_selection = net_choice;
                    Network net;
                    if (!network_init(&net)) {
                        printf(ANSI_ERROR "\n  Network subsystem failed to initialize.\n" ANSI_RESET);
                        sound_play(&global_sound, SOUND_INVALID);
                        sleep_seconds(2);
                        continue;
                    }

                    play_network(&net, net_choice == 0);
                }
                break;
            }

            case 3: {
                cli_print_highscores(&global_score);
                get_input(input, sizeof(input));
                break;
            }

            case 4: {
                static const int timer_cycle[] = {0, 10, 15, 30, 45, 60};
                const int timer_cycle_count = (int)(sizeof(timer_cycle) / sizeof(timer_cycle[0]));
                int settings_selection = 0;
                bool settings_done = false;

                while (!settings_done) {
                    cli_print_settings_menu(&global_config, settings_selection);
                    CliKey key = cli_read_menu_key();

                    if (key == CLI_KEY_UP) {
                        settings_selection = (settings_selection + 5) % 6;
                    } else if (key == CLI_KEY_DOWN) {
                        settings_selection = (settings_selection + 1) % 6;
                    } else if (key == CLI_KEY_ESCAPE || (key == CLI_KEY_ENTER && settings_selection == 5)) {
                        settings_done = true;
                    } else if (key == CLI_KEY_ENTER) {
                        if (settings_selection == 0) {
                            int current_size = global_config.board_size;
                            if (current_size < MIN_BOARD_SIZE || current_size > MAX_BOARD_SIZE) {
                                current_size = MIN_BOARD_SIZE;
                            }

                            int next_size = current_size + 1;
                            if (next_size > MAX_BOARD_SIZE) next_size = MIN_BOARD_SIZE;
                            global_config.board_size = next_size;
                            config_save(&global_config, get_config_path());
                            sound_play(&global_sound, SOUND_MENU);
                        } else if (settings_selection == 1) {
                            int current_theme = global_config.color_theme;
                            if (current_theme < THEME_DEFAULT || current_theme > THEME_RETRO) {
                                current_theme = THEME_DEFAULT;
                            }

                            int next_theme = current_theme + 1;
                            if (next_theme > THEME_RETRO) next_theme = THEME_DEFAULT;
                            global_config.color_theme = next_theme;
                            cli_set_theme((ColorTheme)next_theme);
                            config_save(&global_config, get_config_path());
                            sound_play(&global_sound, SOUND_MENU);
                        } else if (settings_selection == 2) {
                            int current = 0;
                            if (global_config.timer_enabled && global_config.timer_seconds > 0) {
                                current = global_config.timer_seconds;
                            }

                            int idx = 0;
                            for (int i = 0; i < timer_cycle_count; i++) {
                                if (timer_cycle[i] == current) {
                                    idx = i;
                                    break;
                                }
                            }
                            idx = (idx + 1) % timer_cycle_count;

                            global_config.timer_seconds = timer_cycle[idx];
                            global_config.timer_enabled = (timer_cycle[idx] > 0);
                            config_save(&global_config, get_config_path());
                            sound_play(&global_sound, SOUND_MENU);
                        } else if (settings_selection == 3) {
                            global_config.player_symbol = (global_config.player_symbol == 'O') ? 'X' : 'O';
                            config_save(&global_config, get_config_path());
                            sound_play(&global_sound, SOUND_MENU);
                        } else if (settings_selection == 4) {
                            global_config.sound_enabled = !global_config.sound_enabled;
                            sound_set_enabled(&global_sound, global_config.sound_enabled);
                            config_save(&global_config, get_config_path());
                            if (global_config.sound_enabled) {
                                sound_play(&global_sound, SOUND_MENU);
                            }
                        }
                    }
                }
                cli_menu_invalidate();
                break;
            }

            case 5: {
                cli_print_about_screen();
                get_input(input, sizeof(input));
                break;
            }

            default:
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

static int run_vertical_menu(void (*render_menu)(int), int option_count, int initial_index) {
    if (!render_menu || option_count <= 0) {
        return -1;
    }

    int selected = initial_index;
    if (selected < 0 || selected >= option_count) {
        selected = 0;
    }

    while (1) {
        render_menu(selected);
        CliKey key = cli_read_menu_key();

        if (key == CLI_KEY_UP) {
            selected = (selected + option_count - 1) % option_count;
        } else if (key == CLI_KEY_DOWN) {
            selected = (selected + 1) % option_count;
        } else if (key == CLI_KEY_ENTER) {
            cli_menu_invalidate();
            return selected;
        } else if (key == CLI_KEY_ESCAPE) {
            cli_menu_invalidate();
            return -1;
        }
    }
}

static void print_welcome_animation(void) {
    cli_print_title();

    printf(ANSI_YELLOW "  Loading");
    fflush(stdout);
    for (int i = 0; i < 3; i++) {
        sleep_milliseconds(220);
        printf(".");
        fflush(stdout);
    }
    printf("\n" ANSI_RESET);
    sleep_milliseconds(120);
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
            printf(ANSI_ERROR "  Invalid input! Use 23 or 2 3.\n" ANSI_RESET);
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
                printf(ANSI_ERROR "  Invalid input! Use 23 or 2 3.\n" ANSI_RESET);
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
    cli_print_ai_thinking();

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
            size_t i = 0;
            while (input[i] != '\0' && i + 1 < sizeof(ip)) {
                ip[i] = input[i];
                i++;
            }
            ip[i] = '\0';
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

    printf(ANSI_BRIGHT_GREEN "\n  Launching GUI mode...\n" ANSI_RESET);
    if (!gui_run_app(&global_config, &global_score, &global_sound)) {
        gui_close(&gui);
        return false;
    }

    score_save(&global_score, get_highscore_path());
    config_save(&global_config, get_config_path());
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

static void sleep_milliseconds(unsigned int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000U);
#endif
}

static void print_cli_usage(const char* program_name) {
    const char* app = (program_name && program_name[0] != '\0') ? program_name : "tictactoe-cx";
    printf("%s v%s\n", APP_NAME, APP_VERSION);
    printf("Usage: %s [--gui|-g] [--version|-v] [--help|-h]\n", app);
    printf("  --gui, -g      Launch GUI mode when available\n");
    printf("  --version, -v  Print version and exit\n");
    printf("  --help, -h     Show this help message\n");
}

static void print_version(void) {
    printf("%s v%s\n", APP_NAME, APP_VERSION);
}

static bool parse_move_input(const char* input, uint8_t board_size, uint8_t* row, uint8_t* col) {
    if (!input || !row || !col) {
        return false;
    }

    int user_row = 0;
    int user_col = 0;
    int consumed = 0;

    if (sscanf(input, " %d %d %n", &user_row, &user_col, &consumed) == 2) {
        while (input[consumed] != '\0' && isspace((unsigned char)input[consumed])) {
            consumed++;
        }
        if (input[consumed] == '\0') {
            if (user_row < 1 || user_col < 1 || user_row > board_size || user_col > board_size) {
                return false;
            }

            *row = (uint8_t)(user_row - 1);
            *col = (uint8_t)(user_col - 1);
            return true;
        }
    }

    char compact[16];
    size_t compact_len = 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (isspace((unsigned char)input[i])) {
            continue;
        }

        if (!isdigit((unsigned char)input[i])) {
            return false;
        }

        if (compact_len + 1 >= sizeof(compact)) {
            return false;
        }
        compact[compact_len++] = input[i];
    }
    compact[compact_len] = '\0';

    if (compact_len != 2) {
        return false;
    }

    user_row = compact[0] - '0';
    user_col = compact[1] - '0';

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
