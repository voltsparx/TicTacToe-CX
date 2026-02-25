#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include "game.h"

typedef struct {
    int board_size;
    int ai_difficulty;
    int timer_seconds;
    bool timer_enabled;
    char player_symbol;
    int color_theme;
    bool sound_enabled;
} Config;

typedef struct {
    int wins;
    int losses;
    int draws;
} Score;

void config_init(Config* cfg);
bool config_load(Config* cfg, const char* filepath);
bool config_save(Config* cfg, const char* filepath);

void score_init(Score* score);
bool score_load(Score* score, const char* filepath);
bool score_save(Score* score, const char* filepath);
void score_update(Score* score, int result);

bool init_data_paths(bool interactive_prompt);
const char* get_data_root_path(void);
const char* get_config_path(void);
const char* get_highscore_path(void);

#endif
