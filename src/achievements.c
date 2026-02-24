#include "achievements.h"
#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const Achievement all_achievements[MAX_ACHIEVEMENTS] = {
    {ACHIEVEMENT_FIRST_WIN, "First Victory", "Win your first game", false, 0},
    {ACHIEVEMENT_WIN_5_GAMES, "Getting Good", "Win 5 games", false, 0},
    {ACHIEVEMENT_WIN_10_GAMES, "Pro Player", "Win 10 games", false, 0},
    {ACHIEVEMENT_WIN_25_GAMES, "Master Mind", "Win 25 games", false, 0},
    {ACHIEVEMENT_UNBEATABLE_BEATEN, "Giant Killer", "Beat the Hard AI", false, 0},
    {ACHIEVEMENT_DRAW_MASTER, "Draw Master", "Achieve 5 draws", false, 0},
    {ACHIEVEMENT_SPEED_DEMON, "Speed Demon", "Win in under 30 seconds", false, 0},
    {ACHIEVEMENT_PERFECT_GAME, "Perfect Game", "Win without letting opponent mark center", false, 0},
    {ACHIEVEMENT_COMEBACK_KID, "Comeback Kid", "Win after being behind", false, 0},
    {ACHIEVEMENT_NO_MERCY, "No Mercy", "Win against AI without making mistakes", false, 0},
    {ACHIEVEMENT_LUCKY_STRIKE, "Lucky Strike", "Win with a corner move", false, 0},
    {ACHIEVEMENT_CLUTCH_PLAYER, "Clutch Player", "Win on the last possible move", false, 0},
    {ACHIEVEMENT_STREAK_3, "Heating Up", "Win 3 games in a row", false, 0},
    {ACHIEVEMENT_STREAK_5, "On Fire", "Win 5 games in a row", false, 0},
    {ACHIEVEMENT_STREAK_10, "Unstoppable", "Win 10 games in a row", false, 0},
    {ACHIEVEMENT_VARIETY_PLAYER, "Jack of All Trades", "Play all game modes", false, 0},
    {ACHIEVEMENT_SIZE_MASTER, "Size Master", "Win on all board sizes", false, 0},
    {ACHIEVEMENT_TIMER_CHAMPION, "Timer Champion", "Win 10 timed games", false, 0},
    {ACHIEVEMENT_SOCIAL_PLAYER, "Social Player", "Play 5 LAN multiplayer games", false, 0},
    {ACHIEVEMENT_CUSTOM_CHAMP, "Custom Champion", "Win using custom symbols", false, 0}
};

void achievements_init(AchievementsData* data) {
    if (!data) return;
    
    memset(data, 0, sizeof(AchievementsData));
    
    for (int i = 0; i < MAX_ACHIEVEMENTS; i++) {
        data->achievements[i] = all_achievements[i];
    }
    
    data->total_achievements = MAX_ACHIEVEMENTS;
    data->first_play_time = time(NULL);
}

bool achievements_load(AchievementsData* data, const char* filepath) {
    if (!data || !filepath) return false;

    achievements_init(data);
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        return false;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        int type;
        int unlocked = 0;
        if (sscanf(line, "%d %d", &type, &unlocked) == 2) {
            if (type >= 0 && type < MAX_ACHIEVEMENTS) {
                data->achievements[type].unlocked = (unlocked != 0);
                if (unlocked != 0) {
                    data->unlocked_count++;
                }
            }
        } else if (sscanf(line, "wins %d", &data->wins) == 1) {
            continue;
        } else if (sscanf(line, "losses %d", &data->losses) == 1) {
            continue;
        } else if (sscanf(line, "draws %d", &data->draws) == 1) {
            continue;
        } else if (sscanf(line, "current_streak %d", &data->current_streak) == 1) {
            continue;
        } else if (sscanf(line, "best_streak %d", &data->best_streak) == 1) {
            continue;
        }
    }
    
    fclose(f);
    return true;
}

bool achievements_save(AchievementsData* data, const char* filepath) {
    if (!data || !filepath) return false;
    
    FILE* f = fopen(filepath, "w");
    if (!f) return false;
    
    fprintf(f, "# Achievements - TicTacToe-CX\n");
    
    for (int i = 0; i < data->total_achievements; i++) {
        fprintf(f, "%d %d\n", data->achievements[i].type, data->achievements[i].unlocked);
    }
    
    fprintf(f, "# Stats\n");
    fprintf(f, "wins %d\n", data->wins);
    fprintf(f, "losses %d\n", data->losses);
    fprintf(f, "draws %d\n", data->draws);
    fprintf(f, "current_streak %d\n", data->current_streak);
    fprintf(f, "best_streak %d\n", data->best_streak);
    
    fclose(f);
    return true;
}

void achievements_unlock(AchievementsData* data, AchievementType type) {
    if (!data) return;
    if (type < 0 || type >= MAX_ACHIEVEMENTS) return;
    
    if (!data->achievements[type].unlocked) {
        data->achievements[type].unlocked = true;
        data->achievements[type].unlock_time = time(NULL);
        data->unlocked_count++;
        
        printf(ANSI_GREEN "\n  [ACHIEVEMENT UNLOCKED!]\n");
        printf(ANSI_YELLOW "  %s: %s\n\n" ANSI_RESET, 
               data->achievements[type].name, 
               data->achievements[type].description);
    }
}

void achievements_check(AchievementsData* data, Game* game) {
    if (!data || !game) return;
    
    if (game->state == GAME_STATE_WIN) {
        Player winner = game_get_winner(game);
        bool player_won = (winner == game->player_symbol);
        
        if (player_won) {
            data->wins++;
            data->current_streak++;
            
            if (data->current_streak > data->best_streak) {
                data->best_streak = data->current_streak;
            }
            
            if (data->wins >= 1) {
                achievements_unlock(data, ACHIEVEMENT_FIRST_WIN);
            }
            if (data->wins >= 5) {
                achievements_unlock(data, ACHIEVEMENT_WIN_5_GAMES);
            }
            if (data->wins >= 10) {
                achievements_unlock(data, ACHIEVEMENT_WIN_10_GAMES);
            }
            if (data->wins >= 25) {
                achievements_unlock(data, ACHIEVEMENT_WIN_25_GAMES);
            }
            
            if (game->mode == MODE_AI_HARD) {
                achievements_unlock(data, ACHIEVEMENT_UNBEATABLE_BEATEN);
            }
            
            if (data->current_streak >= 3) {
                achievements_unlock(data, ACHIEVEMENT_STREAK_3);
            }
            if (data->current_streak >= 5) {
                achievements_unlock(data, ACHIEVEMENT_STREAK_5);
            }
            if (data->current_streak >= 10) {
                achievements_unlock(data, ACHIEVEMENT_STREAK_10);
            }
            
            const int board_cells = game->size * game->size;
            if (game->move_count >= board_cells - 1) {
                achievements_unlock(data, ACHIEVEMENT_CLUTCH_PLAYER);
            }
            
            if (game->size % 2 == 1) {
                int center = game->size / 2;
                if (game->board[center][center] == PLAYER_NONE || game->board[center][center] == winner) {
                    achievements_unlock(data, ACHIEVEMENT_PERFECT_GAME);
                }
            }
        } else {
            data->losses++;
            data->current_streak = 0;
        }
    } else if (game->state == GAME_STATE_DRAW) {
        data->draws++;
        data->current_streak = 0;
        
        if (data->draws >= 5) {
            achievements_unlock(data, ACHIEVEMENT_DRAW_MASTER);
        }
    }
}

void achievements_print(AchievementsData* data) {
    if (!data) return;
    
    printf(ANSI_BOLD "\n═══════════════════════════════════════\n");
    printf(       "         " ANSI_YELLOW " ACHIEVEMENTS " ANSI_RESET "\n");
    printf(ANSI_BOLD "═══════════════════════════════════════\n\n");
    
    printf(ANSI_CYAN "  Unlocked: %d / %d\n\n" ANSI_RESET, 
           data->unlocked_count, data->total_achievements);
    
    for (int i = 0; i < data->total_achievements; i++) {
        if (data->achievements[i].unlocked) {
            printf(ANSI_GREEN "  [%c] " ANSI_RESET "%s\n", 
                   '*', data->achievements[i].name);
            printf(ANSI_WHITE "        %s\n\n", 
                   data->achievements[i].description);
        } else {
            printf(ANSI_GRAY "  [ ] " ANSI_RESET "%s\n", 
                   data->achievements[i].name);
        }
    }
    
    printf(ANSI_BOLD "═══════════════════════════════════════\n");
    printf("  Stats: W:%d L:%d D:%d | Best Streak: %d\n\n" ANSI_RESET,
           data->wins, data->losses, data->draws, data->best_streak);
}

int achievements_get_unlocked_count(AchievementsData* data) {
    return data ? data->unlocked_count : 0;
}

bool achievements_is_unlocked(AchievementsData* data, AchievementType type) {
    if (!data || type < 0 || type >= MAX_ACHIEVEMENTS) return false;
    return data->achievements[type].unlocked;
}
