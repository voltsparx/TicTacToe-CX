#ifndef GAME_H
#define GAME_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_BOARD_SIZE 5
#define MIN_BOARD_SIZE 3
#define MAX_MOVES 25
#define MAX_UNDO_HISTORY 10

typedef enum {
    PLAYER_NONE = 0,
    PLAYER_X = 1,
    PLAYER_O = 2
} Player;

typedef enum {
    GAME_STATE_PLAYING,
    GAME_STATE_WIN,
    GAME_STATE_DRAW,
    GAME_STATE_WAITING
} GameState;

typedef enum {
    MODE_LOCAL_2P,
    MODE_AI_EASY,
    MODE_AI_MEDIUM,
    MODE_AI_HARD,
    MODE_NETWORK_HOST,
    MODE_NETWORK_CLIENT
} GameMode;

typedef struct {
    uint8_t row;
    uint8_t col;
    Player player;
} Move;

typedef struct {
    uint8_t board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    uint8_t size;
    Player current_player;
    Player player_symbol;
    GameState state;
    GameMode mode;
    int move_count;
    int8_t win_line[4];
    bool timer_enabled;
    int time_per_move;
    int time_remaining;
    
    char symbol_x;
    char symbol_o;
    
    Move move_history[MAX_MOVES];
    int history_count;
    int undo_count;
} Game;

void game_init(Game* game, uint8_t size, GameMode mode);
void game_reset(Game* game);
bool game_make_move(Game* game, uint8_t row, uint8_t col);
Player game_check_winner(Game* game);
bool game_is_board_full(Game* game);
void game_switch_player(Game* game);
char game_get_cell_char(Game* game, uint8_t row, uint8_t col);
bool game_undo(Game* game);
bool game_redo(Game* game);
bool game_can_undo(Game* game);
bool game_can_redo(Game* game);
void game_set_custom_symbols(Game* game, char symbol_x, char symbol_o);
void game_start_timer(Game* game, int seconds);
bool game_update_timer(Game* game);
int game_get_timer_remaining(Game* game);
void game_clear_history(Game* game);

#endif
