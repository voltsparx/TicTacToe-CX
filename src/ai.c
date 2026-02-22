#include "ai.h"
#include <stdlib.h>
#include <limits.h>
#include <time.h>

static Move findWinningMove(Game* game, Player player);
static int minimax(Game* game, int depth, Player ai_player, Player human_player, int alpha, int beta);

void ai_get_move(Game* game, Move* move) {
    if (!game || !move) return;
    
    if (game->mode < MODE_AI_EASY || game->mode > MODE_AI_HARD) return;
    
    const uint8_t n = game->size;
    const Player ai_player = game->current_player;
    const Player human_player = (ai_player == PLAYER_X) ? PLAYER_O : PLAYER_X;
    
    move->row = n;
    move->col = n;
    
    if (game->mode == MODE_AI_EASY) {
        uint8_t available[MAX_MOVES];
        uint8_t count = 0;
        
        for (uint8_t i = 0; i < n; i++) {
            for (uint8_t j = 0; j < n; j++) {
                if (game->board[i][j] == PLAYER_NONE) {
                    available[count++] = (uint8_t)(i * n + j);
                }
            }
        }
        
        if (count > 0) {
            const uint8_t idx = (uint8_t)(rand() % count);
            move->row = available[idx] / n;
            move->col = available[idx] % n;
        }
        return;
    }
    
    if (game->mode == MODE_AI_MEDIUM) {
        Move win_move = findWinningMove(game, ai_player);
        if (win_move.row < n && win_move.col < n) {
            *move = win_move;
            return;
        }
        
        Move block_move = findWinningMove(game, human_player);
        if (block_move.row < n && block_move.col < n) {
            *move = block_move;
            return;
        }
        
        if (n > 2 && game->board[1][1] == PLAYER_NONE) {
            move->row = 1;
            move->col = 1;
            return;
        }
        
        static const uint8_t corners[4][2] = {{0, 0}, {0, 255}, {255, 0}, {255, 255}};
        for (int i = 0; i < 4; i++) {
            const uint8_t r = (n == 3) ? corners[i][0] : (corners[i][0] ? n - 1 : 0);
            const uint8_t c = (n == 3) ? corners[i][1] : (corners[i][1] ? n - 1 : 0);
            if (game->board[r][c] == PLAYER_NONE) {
                move->row = r;
                move->col = c;
                return;
            }
        }
        
        uint8_t available[MAX_MOVES];
        uint8_t count = 0;
        for (uint8_t i = 0; i < n; i++) {
            for (uint8_t j = 0; j < n; j++) {
                if (game->board[i][j] == PLAYER_NONE) {
                    available[count++] = (uint8_t)(i * n + j);
                }
            }
        }
        
        if (count > 0) {
            const uint8_t idx = (uint8_t)(rand() % count);
            move->row = available[idx] / n;
            move->col = available[idx] % n;
        }
        return;
    }
    
    if (game->mode == MODE_AI_HARD) {
        int bestScore = INT_MIN;
        
        for (uint8_t i = 0; i < n; i++) {
            for (uint8_t j = 0; j < n; j++) {
                if (game->board[i][j] != PLAYER_NONE) continue;
                
                game->board[i][j] = ai_player;
                
                const Player winner = game_check_winner(game);
                int score;
                if (winner == ai_player) {
                    score = 100;
                } else if (winner == human_player) {
                    score = -100;
                } else if (game_is_board_full(game)) {
                    score = 0;
                } else {
                    score = -minimax(game, 1, ai_player, human_player, INT_MIN, INT_MAX);
                }
                
                game->board[i][j] = PLAYER_NONE;
                
                if (score > bestScore) {
                    bestScore = score;
                    move->row = i;
                    move->col = j;
                    if (score == 100) return;
                }
            }
        }
    }
}

static Move findWinningMove(Game* game, Player player) {
    Move move = {MAX_BOARD_SIZE, MAX_BOARD_SIZE};
    const uint8_t n = game->size;
    
    for (uint8_t i = 0; i < n; i++) {
        for (uint8_t j = 0; j < n; j++) {
            if (game->board[i][j] != PLAYER_NONE) continue;
            
            game->board[i][j] = player;
            const Player winner = game_check_winner(game);
            game->board[i][j] = PLAYER_NONE;
            
            if (winner == player) {
                move.row = i;
                move.col = j;
                return move;
            }
        }
    }
    
    return move;
}

static int minimax(Game* game, int depth, Player ai_player, Player human_player, int alpha, int beta) {
    const uint8_t n = game->size;
    
    const Player winner = game_check_winner(game);
    if (winner == ai_player) return 100 - depth;
    if (winner == human_player) return -100 + depth;
    if (game_is_board_full(game)) return 0;
    
    if (depth >= 6) return 0;
    
    int maxScore = INT_MIN;
    
    for (uint8_t i = 0; i < n; i++) {
        for (uint8_t j = 0; j < n; j++) {
            if (game->board[i][j] != PLAYER_NONE) continue;
            
            game->board[i][j] = ai_player;
            
            const Player w = game_check_winner(game);
            int score;
            if (w == ai_player) {
                score = 100 - depth;
            } else if (w == human_player) {
                score = -100 + depth;
            } else if (game_is_board_full(game)) {
                score = 0;
            } else {
                score = -minimax(game, depth + 1, ai_player, human_player, -beta, -alpha);
            }
            
            game->board[i][j] = PLAYER_NONE;
            
            if (score > maxScore) {
                maxScore = score;
            }
            if (score > alpha) {
                alpha = score;
            }
            if (beta <= alpha) {
                return maxScore;
            }
        }
    }
    
    return maxScore;
}
