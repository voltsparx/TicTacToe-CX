#include "game.h"
#include <string.h>
#include <stdlib.h>

void game_init(Game* game, uint8_t size, GameMode mode) {
    memset(game, 0, sizeof(Game));
    game->size = (size < MIN_BOARD_SIZE) ? 3 : (size > MAX_BOARD_SIZE) ? MAX_BOARD_SIZE : size;
    game->mode = mode;
    game->current_player = PLAYER_X;
    game->player_symbol = PLAYER_X;
    game->state = GAME_STATE_PLAYING;
    game->timer_enabled = false;
    game->time_per_move = 30;
    game->time_remaining = 30;
    game->symbol_x = 'X';
    game->symbol_o = 'O';
    
    for (int i = 0; i < 4; i++) {
        game->win_line[i] = -1;
    }
}

void game_reset(Game* game) {
    memset(game->board, 0, sizeof(game->board));
    game->current_player = PLAYER_X;
    game->player_symbol = PLAYER_X;
    game->state = GAME_STATE_PLAYING;
    game->move_count = 0;
    game->history_count = 0;
    game->undo_count = 0;
    
    if (game->timer_enabled) {
        game->time_remaining = game->time_per_move;
    }
    
    for (int i = 0; i < 4; i++) {
        game->win_line[i] = -1;
    }
}

void game_clear_history(Game* game) {
    game->history_count = 0;
    game->undo_count = 0;
}

bool game_make_move(Game* game, uint8_t row, uint8_t col) {
    if (row >= game->size || col >= game->size) {
        return false;
    }
    
    if (game->board[row][col] != PLAYER_NONE) {
        return false;
    }
    
    if (game->state != GAME_STATE_PLAYING && game->state != GAME_STATE_WAITING) {
        return false;
    }
    
    if (game->undo_count > 0) {
        game->history_count -= game->undo_count;
        game->undo_count = 0;
    }
    
    if (game->history_count < MAX_MOVES) {
        game->move_history[game->history_count].row = row;
        game->move_history[game->history_count].col = col;
        game->move_history[game->history_count].player = game->current_player;
        game->history_count++;
    }
    
    game->board[row][col] = game->current_player;
    game->move_count++;
    
    Player winner = game_check_winner(game);
    
    if (winner != PLAYER_NONE) {
        game->state = GAME_STATE_WIN;
    } else if (game_is_board_full(game)) {
        game->state = GAME_STATE_DRAW;
    } else {
        game_switch_player(game);
        if (game->timer_enabled) {
            game->time_remaining = game->time_per_move;
        }
    }
    
    return true;
}

Player game_check_winner(Game* game) {
    const uint8_t n = game->size;
    const uint8_t win_len = (n == 3) ? 3 : 4;
    
    for (int i = 0; i < 4; i++) {
        game->win_line[i] = -1;
    }
    
    static const int8_t dirs[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
    
    for (uint8_t r = 0; r < n; r++) {
        for (uint8_t c = 0; c < n; c++) {
            const Player p = game->board[r][c];
            if (p == PLAYER_NONE) continue;
            
            for (int d = 0; d < 4; d++) {
                const int8_t dr = dirs[d][0];
                const int8_t dc = dirs[d][1];
                
                uint8_t count = 1;
                int8_t sr = r, sc = c, er = r, ec = c;
                
                int8_t nr = r + dr;
                int8_t nc = c + dc;
                while (nr >= 0 && nr < n && nc >= 0 && nc < n && game->board[nr][nc] == p) {
                    count++;
                    er = nr;
                    ec = nc;
                    nr += dr;
                    nc += dc;
                }
                
                nr = r - dr;
                nc = c - dc;
                while (nr >= 0 && nr < n && nc >= 0 && nc < n && game->board[nr][nc] == p) {
                    count++;
                    sr = nr;
                    sc = nc;
                    nr -= dr;
                    nc -= dc;
                }
                
                if (count >= win_len) {
                    game->win_line[0] = sr;
                    game->win_line[1] = sc;
                    game->win_line[2] = er;
                    game->win_line[3] = ec;
                    return p;
                }
            }
        }
    }
    
    return PLAYER_NONE;
}

bool game_is_board_full(Game* game) {
    const uint8_t n = game->size;
    for (uint8_t i = 0; i < n; i++) {
        for (uint8_t j = 0; j < n; j++) {
            if (game->board[i][j] == PLAYER_NONE) {
                return false;
            }
        }
    }
    return true;
}

void game_switch_player(Game* game) {
    game->current_player = (game->current_player == PLAYER_X) ? PLAYER_O : PLAYER_X;
}

char game_get_cell_char(const Game* game, uint8_t row, uint8_t col) {
    const Player p = game->board[row][col];
    if (p == PLAYER_X) return game->symbol_x;
    if (p == PLAYER_O) return game->symbol_o;
    return ' ';
}

bool game_undo(Game* game) {
    if (game->history_count == 0) return false;
    if (game->state != GAME_STATE_PLAYING) return false;
    if (game->mode >= MODE_AI_EASY && game->mode <= MODE_AI_HARD) {
        if (game->move_count <= 1) return false;
    }
    
    game->history_count--;
    game->undo_count++;
    
    const uint8_t row = game->move_history[game->history_count].row;
    const uint8_t col = game->move_history[game->history_count].col;
    const Player player = game->move_history[game->history_count].player;
    
    game->board[row][col] = PLAYER_NONE;
    game->move_count--;
    game->current_player = player;
    game->state = GAME_STATE_PLAYING;
    
    for (int i = 0; i < 4; i++) {
        game->win_line[i] = -1;
    }
    
    return true;
}

bool game_redo(Game* game) {
    if (game->undo_count == 0) return false;
    if (game->state != GAME_STATE_PLAYING) return false;
    
    const uint8_t row = game->move_history[game->history_count].row;
    const uint8_t col = game->move_history[game->history_count].col;
    const Player player = game->move_history[game->history_count].player;
    
    game->board[row][col] = player;
    game->move_count++;
    game->history_count++;
    game->undo_count--;
    
    const Player winner = game_check_winner(game);
    if (winner != PLAYER_NONE) {
        game->state = GAME_STATE_WIN;
    } else if (game_is_board_full(game)) {
        game->state = GAME_STATE_DRAW;
    } else {
        game_switch_player(game);
    }
    
    return true;
}

bool game_can_undo(Game* game) {
    if (game->history_count == 0) return false;
    if (game->state != GAME_STATE_PLAYING) return false;
    if (game->mode >= MODE_AI_EASY && game->mode <= MODE_AI_HARD) {
        return game->move_count > 1;
    }
    return true;
}

bool game_can_redo(Game* game) {
    return game->undo_count > 0 && game->state == GAME_STATE_PLAYING;
}

void game_set_custom_symbols(Game* game, char symbol_x, char symbol_o) {
    game->symbol_x = symbol_x;
    game->symbol_o = symbol_o;
}

void game_start_timer(Game* game, int seconds) {
    game->timer_enabled = (seconds > 0);
    game->time_per_move = seconds;
    game->time_remaining = seconds;
}

bool game_update_timer(Game* game) {
    if (!game->timer_enabled) return false;
    if (game->state != GAME_STATE_PLAYING) return false;
    
    game->time_remaining--;
    
    if (game->time_remaining <= 0) {
        game->state = GAME_STATE_WIN;
        game->current_player = (game->current_player == PLAYER_X) ? PLAYER_O : PLAYER_X;
        return true;
    }
    
    return false;
}

int game_get_timer_remaining(Game* game) {
    return game->time_remaining;
}

Player game_get_winner(const Game* game) {
    if (!game || game->state != GAME_STATE_WIN) {
        return PLAYER_NONE;
    }
    return game->current_player;
}
