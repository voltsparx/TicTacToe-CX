#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_init(Config* cfg) {
    if (!cfg) return;
    cfg->board_size = 3;
    cfg->ai_difficulty = 2;
    cfg->timer_seconds = 30;
    cfg->timer_enabled = false;
    cfg->player_symbol = 'X';
    cfg->color_theme = 0;
}

bool config_load(Config* cfg, const char* filepath) {
    if (!cfg || !filepath) return false;
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        config_init(cfg);
        return false;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            if (strcmp(key, "board_size") == 0) {
                cfg->board_size = atoi(value);
            } else if (strcmp(key, "ai_difficulty") == 0) {
                cfg->ai_difficulty = atoi(value);
            } else if (strcmp(key, "timer_seconds") == 0) {
                cfg->timer_seconds = atoi(value);
            } else if (strcmp(key, "timer_enabled") == 0) {
                cfg->timer_enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            } else if (strcmp(key, "player_symbol") == 0) {
                cfg->player_symbol = value[0];
            } else if (strcmp(key, "color_theme") == 0) {
                cfg->color_theme = atoi(value);
            }
        }
    }
    
    fclose(f);
    return true;
}

bool config_save(Config* cfg, const char* filepath) {
    if (!cfg || !filepath) return false;
    
    FILE* f = fopen(filepath, "w");
    if (!f) return false;
    
    fprintf(f, "board_size=%d\n", cfg->board_size);
    fprintf(f, "ai_difficulty=%d\n", cfg->ai_difficulty);
    fprintf(f, "timer_seconds=%d\n", cfg->timer_seconds);
    fprintf(f, "timer_enabled=%s\n", cfg->timer_enabled ? "true" : "false");
    fprintf(f, "player_symbol=%c\n", cfg->player_symbol);
    fprintf(f, "color_theme=%d\n", cfg->color_theme);
    
    fclose(f);
    return true;
}

void score_init(Score* score) {
    if (!score) return;
    score->wins = 0;
    score->losses = 0;
    score->draws = 0;
}

bool score_load(Score* score, const char* filepath) {
    if (!score || !filepath) return false;
    
    FILE* f = fopen(filepath, "r");
    if (!f) {
        score_init(score);
        return false;
    }
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char mode[64];
        int w, l, d;
        if (sscanf(line, "%63[^:]: %d %d %d", mode, &w, &l, &d) == 4) {
            if (strcmp(mode, "total") == 0) {
                score->wins = w;
                score->losses = l;
                score->draws = d;
            }
        }
    }
    
    fclose(f);
    return true;
}

bool score_save(Score* score, const char* filepath) {
    if (!score || !filepath) return false;
    
    FILE* f = fopen(filepath, "w");
    if (!f) return false;
    
    fprintf(f, "total: %d %d %d\n", score->wins, score->losses, score->draws);
    fprintf(f, "# Format: mode: wins losses draws\n");
    
    fclose(f);
    return true;
}

void score_update(Score* score, int result) {
    if (!score) return;
    if (result > 0) score->wins++;
    else if (result < 0) score->losses++;
    else score->draws++;
}

char* get_config_path(void) {
    return "config/config.ini";
}

char* get_highscore_path(void) {
    return "saves/highscores.txt";
}
