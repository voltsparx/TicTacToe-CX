#include "replay.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

void replay_init(Replay* replay) {
    if (!replay) return;
    memset(replay, 0, sizeof(Replay));
    replay->game_time = time(NULL);
    replay->current_step = -1;
}

void replay_start(Replay* replay, Game* game) {
    if (!replay || !game) return;
    
    memset(replay, 0, sizeof(Replay));
    replay->size = game->size;
    replay->mode = game->mode;
    replay->winner = PLAYER_NONE;
    replay->game_time = time(NULL);
    replay->current_step = -1;
}

bool replay_add_move(Replay* replay, uint8_t row, uint8_t col, Player player) {
    if (!replay) return false;
    if (replay->move_count >= MAX_REPLAY_MOVES) return false;
    
    replay->moves[replay->move_count].row = row;
    replay->moves[replay->move_count].col = col;
    replay->moves[replay->move_count].player = player;
    replay->move_count++;
    
    return true;
}

bool replay_save(Replay* replay, const char* filepath) {
    if (!replay || !filepath) return false;
    
    FILE* f = fopen(filepath, "w");
    if (!f) return false;
    
    fprintf(f, "# TicTacToe-CX Replay\n");
    fprintf(f, "size %d\n", replay->size);
    fprintf(f, "mode %d\n", replay->mode);
    fprintf(f, "moves %d\n", replay->move_count);
    fprintf(f, "# Moves (row col player)\n");
    
    for (int i = 0; i < replay->move_count; i++) {
        fprintf(f, "%d %d %d\n", 
                replay->moves[i].row, 
                replay->moves[i].col, 
                replay->moves[i].player);
    }
    
    fclose(f);
    return true;
}

bool replay_load(Replay* replay, const char* filepath) {
    if (!replay || !filepath) return false;
    
    FILE* f = fopen(filepath, "r");
    if (!f) return false;
    
    char line[256];
    replay->move_count = 0;
    replay->current_step = -1;
    
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        if (strncmp(line, "size ", 5) == 0) {
            sscanf(line + 5, "%d", &replay->size);
        } else if (strncmp(line, "mode ", 5) == 0) {
            sscanf(line + 5, "%d", &replay->mode);
        } else {
            int r, c, p;
            if (sscanf(line, "%d %d %d", &r, &c, &p) == 3) {
                if (replay->move_count < MAX_REPLAY_MOVES) {
                    replay->moves[replay->move_count].row = (uint8_t)r;
                    replay->moves[replay->move_count].col = (uint8_t)c;
                    replay->moves[replay->move_count].player = (Player)p;
                    replay->move_count++;
                }
            }
        }
    }
    
    fclose(f);
    return true;
}

void replay_rewind(Replay* replay) {
    if (!replay) return;
    memset(replay->board, 0, sizeof(replay->board));
    replay->current_player = PLAYER_X;
    replay->current_step = -1;
}

void replay_step_forward(Replay* replay) {
    if (!replay) return;
    if (replay->current_step >= (int)replay->move_count - 1) return;
    
    replay->current_step++;
    Move* m = &replay->moves[replay->current_step];
    replay->board[m->row][m->col] = m->player;
    replay->current_player = (m->player == PLAYER_X) ? PLAYER_O : PLAYER_X;
}

void replay_step_back(Replay* replay) {
    if (!replay) return;
    if (replay->current_step < 0) return;
    
    Move* m = &replay->moves[replay->current_step];
    replay->board[m->row][m->col] = PLAYER_NONE;
    replay->current_player = m->player;
    replay->current_step--;
}

bool replay_is_at_end(Replay* replay) {
    if (!replay) return true;
    return replay->current_step >= (int)replay->move_count - 1;
}

bool replay_is_at_start(Replay* replay) {
    if (!replay) return true;
    return replay->current_step < 0;
}

void replay_history_init(ReplayHistory* history) {
    if (!history) return;
    memset(history, 0, sizeof(ReplayHistory));
}

bool replay_history_add(ReplayHistory* history, Replay* replay) {
    if (!history || !replay) return false;
    if (history->count >= MAX_REPLAY_MOVES) return false;
    
    history->replays[history->count] = *replay;
    history->count++;
    
    return true;
}

Replay* replay_history_get(ReplayHistory* history, int index) {
    if (!history) return NULL;
    if (index < 0 || index >= history->count) return NULL;
    
    return &history->replays[index];
}

int replay_history_get_count(ReplayHistory* history) {
    return history ? history->count : 0;
}

bool replay_history_save(ReplayHistory* history, const char* filepath) {
    if (!history || !filepath) return false;
    
    FILE* f = fopen(filepath, "w");
    if (!f) return false;
    
    fprintf(f, "# TicTacToe-CX Replay History\n");
    fprintf(f, "count %d\n", history->count);
    
    for (int i = 0; i < history->count; i++) {
        Replay* r = &history->replays[i];
        fprintf(f, "\n# Replay %d\n", i + 1);
        fprintf(f, "size %d\n", r->size);
        fprintf(f, "mode %d\n", r->mode);
        fprintf(f, "moves %d\n", r->move_count);
        
        for (int j = 0; j < r->move_count; j++) {
            fprintf(f, "%d %d %d\n", 
                    r->moves[j].row, 
                    r->moves[j].col, 
                    r->moves[j].player);
        }
    }
    
    fclose(f);
    return true;
}

bool replay_history_load(ReplayHistory* history, const char* filepath) {
    if (!history || !filepath) return false;
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        replay_history_init(history);
        return false;
    }
    
    char line[256];
    int current_replay = -1;
    Replay* r = NULL;
    
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        if (strncmp(line, "count ", 6) == 0) {
            sscanf(line + 6, "%d", &history->count);
            current_replay = -1;
            r = NULL;
        } else if (strncmp(line, "size ", 5) == 0 && r) {
            sscanf(line + 5, "%d", &r->size);
        } else if (strncmp(line, "mode ", 5) == 0 && r) {
            sscanf(line + 5, "%d", &r->mode);
        } else if (strncmp(line, "moves ", 6) == 0 && r) {
            sscanf(line + 6, "%d", &r->move_count);
        } else if (line[0] == '0' || line[0] == '1' || line[0] == '2') {
            int row, col, player;
            if (sscanf(line, "%d %d %d", &row, &col, &player) == 3) {
                if (r && r->move_count < MAX_REPLAY_MOVES) {
                    r->moves[r->move_count].row = (uint8_t)row;
                    r->moves[r->move_count].col = (uint8_t)col;
                    r->moves[r->move_count].player = (Player)player;
                    r->move_count++;
                }
            }
        } else if (strncmp(line, "Replay ", 7) == 0) {
            if (current_replay < history->count - 1) {
                current_replay++;
                r = &history->replays[current_replay];
                r->current_step = -1;
            }
        }
    }
    
    fclose(f);
    return true;
}
