#ifndef ACHIEVEMENTS_H
#define ACHIEVEMENTS_H

#include <stdbool.h>
#include <time.h>
#include "game.h"

#define MAX_ACHIEVEMENTS 20

typedef enum {
    ACHIEVEMENT_FIRST_WIN,
    ACHIEVEMENT_WIN_5_GAMES,
    ACHIEVEMENT_WIN_10_GAMES,
    ACHIEVEMENT_WIN_25_GAMES,
    ACHIEVEMENT_UNBEATABLE_BEATEN,
    ACHIEVEMENT_DRAW_MASTER,
    ACHIEVEMENT_SPEED_DEMON,
    ACHIEVEMENT_PERFECT_GAME,
    ACHIEVEMENT_COMEBACK_KID,
    ACHIEVEMENT_NO_MERCY,
    ACHIEVEMENT_LUCKY_STRIKE,
    ACHIEVEMENT_CLUTCH_PLAYER,
    ACHIEVEMENT_STREAK_3,
    ACHIEVEMENT_STREAK_5,
    ACHIEVEMENT_STREAK_10,
    ACHIEVEMENT_VARIETY_PLAYER,
    ACHIEVEMENT_SIZE_MASTER,
    ACHIEVEMENT_TIMER_CHAMPION,
    ACHIEVEMENT_SOCIAL_PLAYER,
    ACHIEVEMENT_CUSTOM_CHAMP
} AchievementType;

typedef struct {
    AchievementType type;
    const char* name;
    const char* description;
    bool unlocked;
    time_t unlock_time;
} Achievement;

typedef struct {
    Achievement achievements[MAX_ACHIEVEMENTS];
    int total_achievements;
    int unlocked_count;
    int wins;
    int losses;
    int draws;
    int current_streak;
    int best_streak;
    time_t first_play_time;
} AchievementsData;

void achievements_init(AchievementsData* data);
bool achievements_load(AchievementsData* data, const char* filepath);
bool achievements_save(AchievementsData* data, const char* filepath);
void achievements_check(AchievementsData* data, Game* game);
void achievements_unlock(AchievementsData* data, AchievementType type);
void achievements_print(AchievementsData* data);
int achievements_get_unlocked_count(AchievementsData* data);
bool achievements_is_unlocked(AchievementsData* data, AchievementType type);

#endif
