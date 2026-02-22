#ifndef REPLAY_H
#define REPLAY_H

#include "game.h"
#include <stdbool.h>

#define MAX_REPLAY_MOVES 100

typedef struct {
    uint8_t board[MAX_BOARD_SIZE][MAX_BOARD_SIZE];
    uint8_t size;
    Player current_player;
    Player winner;
    GameMode mode;
    int move_count;
    Move moves[MAX_REPLAY_MOVES];
    time_t game_time;
    int current_step;
} Replay;

typedef struct {
    Replay replays[MAX_REPLAY_MOVES];
    int count;
    int current_index;
} ReplayHistory;

void replay_init(Replay* replay);
void replay_start(Replay* replay, Game* game);
bool replay_add_move(Replay* replay, uint8_t row, uint8_t col, Player player);
bool replay_save(Replay* replay, const char* filepath);
bool replay_load(Replay* replay, const char* filepath);
void replay_rewind(Replay* replay);
void replay_step_forward(Replay* replay);
void replay_step_back(Replay* replay);
bool replay_is_at_end(Replay* replay);
bool replay_is_at_start(Replay* replay);

void replay_history_init(ReplayHistory* history);
bool replay_history_add(ReplayHistory* history, Replay* replay);
Replay* replay_history_get(ReplayHistory* history, int index);
int replay_history_get_count(ReplayHistory* history);
bool replay_history_save(ReplayHistory* history, const char* filepath);
bool replay_history_load(ReplayHistory* history, const char* filepath);

#endif
