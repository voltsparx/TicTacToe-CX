#include "utils.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdarg.h>

#ifdef _WIN32
    #include <direct.h>
    #include <io.h>
    #ifndef S_ISDIR
        #define S_ISDIR(mode) (((mode) & _S_IFDIR) == _S_IFDIR)
    #endif
    #define PATH_SEP_CHAR '\\'
    #define MKDIR(path) _mkdir(path)
    #define ISATTY _isatty
    #define FILENO _fileno
#else
    #include <unistd.h>
    #define PATH_SEP_CHAR '/'
    #define MKDIR(path) mkdir(path, 0700)
    #define ISATTY isatty
    #define FILENO fileno
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char g_data_root_path[PATH_MAX] = {0};
static char g_config_path[PATH_MAX] = {0};
static char g_highscore_path[PATH_MAX] = {0};
static bool g_paths_initialized = false;

static bool safe_vsnprintf(char* out, size_t out_size, const char* fmt, va_list args) {
    int written = 0;
    if (!out || out_size == 0 || !fmt) {
        return false;
    }

    written = vsnprintf(out, out_size, fmt, args);
    if (written < 0 || (size_t)written >= out_size) {
        out[0] = '\0';
        return false;
    }
    return true;
}

static bool safe_snprintf(char* out, size_t out_size, const char* fmt, ...) {
    va_list args;
    bool ok = false;

    va_start(args, fmt);
    ok = safe_vsnprintf(out, out_size, fmt, args);
    va_end(args);

    return ok;
}

static bool safe_copy(char* out, size_t out_size, const char* src) {
    if (!src) {
        return false;
    }
    return safe_snprintf(out, out_size, "%s", src);
}

static int clamp_int(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static const char* get_home_dir(void) {
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
    if (home && home[0] != '\0') {
        return home;
    }
    home = getenv("HOME");
    if (home && home[0] != '\0') {
        return home;
    }
#else
    const char* home = getenv("HOME");
    if (home && home[0] != '\0') {
        return home;
    }
#endif
    return NULL;
}

static void trim_whitespace(char* text) {
    if (!text) {
        return;
    }

    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }

    size_t start = 0;
    while (text[start] != '\0' && isspace((unsigned char)text[start])) {
        start++;
    }

    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}

static bool file_exists(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
}

static bool directory_exists(const char* path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode) != 0;
}

static bool ensure_directory_single(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    if (directory_exists(path)) {
        return true;
    }

    if (MKDIR(path) == 0) {
        return true;
    }

    if (errno == EEXIST && directory_exists(path)) {
        return true;
    }

    return false;
}

static bool ensure_directory_recursive(const char* path) {
    char temp[PATH_MAX];
    size_t len = 0;

    if (!path || path[0] == '\0') {
        return false;
    }

    if (!safe_copy(temp, sizeof(temp), path)) {
        return false;
    }
    len = strlen(temp);
    if (len == 0) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        if (temp[i] != '/' && temp[i] != '\\') {
            continue;
        }

        if (i == 0) {
            continue;
        }
#ifdef _WIN32
        if (i == 2 && temp[1] == ':') {
            continue;
        }
#endif

        char saved = temp[i];
        temp[i] = '\0';
        if (!ensure_directory_single(temp)) {
            return false;
        }
        temp[i] = saved;
    }

    return ensure_directory_single(temp);
}

static bool copy_file_if_missing(const char* src, const char* dst) {
    FILE* in = NULL;
    FILE* out = NULL;
    char buffer[4096];
    size_t read_count = 0;

    if (!src || !dst || !file_exists(src) || file_exists(dst)) {
        return true;
    }

    in = fopen(src, "rb");
    if (!in) {
        return false;
    }

    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }

    while ((read_count = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, read_count, out) != read_count) {
            fclose(in);
            fclose(out);
            return false;
        }
    }

    fclose(in);
    fclose(out);
    return true;
}

static bool stdin_is_tty(void) {
    return ISATTY(FILENO(stdin)) != 0;
}

static void build_default_data_root(char* out, size_t out_size) {
    const char* home = get_home_dir();
    if (!out || out_size == 0) {
        return;
    }

    if (home && home[0] != '\0') {
        if (!safe_snprintf(out, out_size, "%s%c.tictactoe-cx-config", home, PATH_SEP_CHAR)) {
            out[0] = '\0';
            return;
        }
    } else {
        if (!safe_snprintf(out, out_size, ".tictactoe-cx-config")) {
            out[0] = '\0';
            return;
        }
    }
}

static void build_state_file_path(char* out, size_t out_size) {
    const char* home = get_home_dir();
    if (!out || out_size == 0) {
        return;
    }

    if (home && home[0] != '\0') {
        if (!safe_snprintf(out, out_size, "%s%c.tictactoe-cx-config-path", home, PATH_SEP_CHAR)) {
            out[0] = '\0';
            return;
        }
    } else {
        if (!safe_snprintf(out, out_size, ".tictactoe-cx-config-path")) {
            out[0] = '\0';
            return;
        }
    }
}

static bool read_state_file(const char* state_path, char* out, size_t out_size) {
    FILE* f = NULL;
    if (!state_path || !out || out_size == 0 || !file_exists(state_path)) {
        return false;
    }

    f = fopen(state_path, "r");
    if (!f) {
        return false;
    }

    if (!fgets(out, (int)out_size, f)) {
        fclose(f);
        out[0] = '\0';
        return false;
    }
    fclose(f);

    trim_whitespace(out);
    return out[0] != '\0';
}

static void write_state_file(const char* state_path, const char* value) {
    FILE* f = NULL;
    if (!state_path || !value || value[0] == '\0') {
        return;
    }

    f = fopen(state_path, "w");
    if (!f) {
        return;
    }

    (void)fprintf(f, "%s\n", value);
    fclose(f);
}

static bool set_data_paths_from_root(const char* root) {
    if (!root) {
        return false;
    }

    if (!safe_copy(g_data_root_path, sizeof(g_data_root_path), root)) {
        return false;
    }
    if (!safe_snprintf(g_config_path, sizeof(g_config_path), "%s%cconfig.ini", root, PATH_SEP_CHAR)) {
        return false;
    }
    if (!safe_snprintf(g_highscore_path, sizeof(g_highscore_path), "%s%csaves%chighscores.txt",
                       root, PATH_SEP_CHAR, PATH_SEP_CHAR)) {
        return false;
    }
    return true;
}

static void migrate_legacy_files(void) {
    (void)copy_file_if_missing("config/config.ini", g_config_path);
    (void)copy_file_if_missing("saves/highscores.txt", g_highscore_path);
}

bool init_data_paths(bool interactive_prompt) {
    char default_root[PATH_MAX] = {0};
    char state_file[PATH_MAX] = {0};
    char persisted_root[PATH_MAX] = {0};
    char chosen_root[PATH_MAX] = {0};
    char saves_dir[PATH_MAX] = {0};

    const char* env_root = NULL;
    bool has_persisted = false;

    if (g_paths_initialized) {
        return true;
    }

    build_default_data_root(default_root, sizeof(default_root));
    build_state_file_path(state_file, sizeof(state_file));
    if (default_root[0] == '\0' || state_file[0] == '\0') {
        return false;
    }

    env_root = getenv("TICTACTOE_CX_HOME");
    if (env_root && env_root[0] != '\0') {
        if (!safe_copy(chosen_root, sizeof(chosen_root), env_root)) {
            return false;
        }
    } else if (read_state_file(state_file, persisted_root, sizeof(persisted_root))) {
        if (!safe_copy(chosen_root, sizeof(chosen_root), persisted_root)) {
            return false;
        }
        has_persisted = true;
    } else {
        if (!safe_copy(chosen_root, sizeof(chosen_root), default_root)) {
            return false;
        }
    }

    if (interactive_prompt && !has_persisted &&
        (!env_root || env_root[0] == '\0') && stdin_is_tty()) {
        char input[PATH_MAX] = {0};
        printf("\nTicTacToe-CX setup\n");
        printf("Choose data directory for config and saves.\n");
        printf("Press Enter to use default:\n  %s\n", default_root);
        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin)) {
            trim_whitespace(input);
            if (input[0] != '\0') {
                if (!safe_copy(chosen_root, sizeof(chosen_root), input)) {
                    return false;
                }
            }
        }
    }

    trim_whitespace(chosen_root);
    if (chosen_root[0] == '\0') {
        if (!safe_copy(chosen_root, sizeof(chosen_root), default_root)) {
            return false;
        }
    }

    if (chosen_root[0] == '~') {
        const char* home = get_home_dir();
        char expanded[PATH_MAX];
        if (home && home[0] != '\0') {
            if (chosen_root[1] == '\0') {
                if (snprintf(expanded, sizeof(expanded), "%s", home) < (int)sizeof(expanded)) {
                    if (!safe_copy(chosen_root, sizeof(chosen_root), expanded)) {
                        return false;
                    }
                }
            } else if (chosen_root[1] == '/' || chosen_root[1] == '\\') {
                if (snprintf(expanded, sizeof(expanded), "%s%s", home, chosen_root + 1) < (int)sizeof(expanded)) {
                    if (!safe_copy(chosen_root, sizeof(chosen_root), expanded)) {
                        return false;
                    }
                }
            }
        }
    }

    if (!set_data_paths_from_root(chosen_root)) {
        return false;
    }

    if (file_exists(g_data_root_path) && !directory_exists(g_data_root_path)) {
        char legacy_config_backup[PATH_MAX];
        if (snprintf(legacy_config_backup, sizeof(legacy_config_backup), "%s.legacy-config.ini", g_data_root_path)
            >= (int)sizeof(legacy_config_backup)) {
            return false;
        }

        if (rename(g_data_root_path, legacy_config_backup) != 0) {
            return false;
        }

        if (!ensure_directory_recursive(g_data_root_path)) {
            return false;
        }

        if (!copy_file_if_missing(legacy_config_backup, g_config_path)) {
            return false;
        }
    }

    if (!ensure_directory_recursive(g_data_root_path)) {
        return false;
    }

    if (snprintf(saves_dir, sizeof(saves_dir), "%s%csaves", g_data_root_path, PATH_SEP_CHAR) >= (int)sizeof(saves_dir)) {
        return false;
    }
    if (!ensure_directory_recursive(saves_dir)) {
        return false;
    }

    migrate_legacy_files();

    if (!file_exists(g_config_path)) {
        Config cfg;
        config_init(&cfg);
        if (!config_save(&cfg, g_config_path)) {
            return false;
        }
    }

    if (!file_exists(g_highscore_path)) {
        Score score;
        score_init(&score);
        if (!score_save(&score, g_highscore_path)) {
            return false;
        }
    }

    if (!env_root || env_root[0] == '\0') {
        write_state_file(state_file, g_data_root_path);
    }

    g_paths_initialized = true;
    return true;
}

void config_init(Config* cfg) {
    if (!cfg) return;
    cfg->board_size = 3;
    cfg->ai_difficulty = 2;
    cfg->timer_seconds = 0;
    cfg->timer_enabled = false;
    cfg->player_symbol = 'X';
    cfg->color_theme = 0;
    cfg->sound_enabled = true;
}

bool config_load(Config* cfg, const char* filepath) {
    if (!cfg || !filepath) return false;

    config_init(cfg);

    FILE* f = fopen(filepath, "r");
    if (!f) {
        return config_save(cfg, filepath);
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
            } else if (strcmp(key, "sound_enabled") == 0) {
                cfg->sound_enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
            }
        }
    }

    cfg->board_size = clamp_int(cfg->board_size, MIN_BOARD_SIZE, MAX_BOARD_SIZE);
    cfg->ai_difficulty = clamp_int(cfg->ai_difficulty, 1, 3);
    cfg->timer_seconds = (cfg->timer_seconds < 0) ? 0 : cfg->timer_seconds;
    if (cfg->player_symbol != 'X' && cfg->player_symbol != 'O') {
        cfg->player_symbol = 'X';
    }
    cfg->color_theme = clamp_int(cfg->color_theme, 0, 3);
    cfg->timer_enabled = cfg->timer_enabled && cfg->timer_seconds > 0;

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
    fprintf(f, "sound_enabled=%s\n", cfg->sound_enabled ? "true" : "false");

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

    score_init(score);

    FILE* f = fopen(filepath, "r");
    if (!f) {
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

const char* get_data_root_path(void) {
    if (!g_paths_initialized) {
        (void)init_data_paths(false);
    }
    return (g_data_root_path[0] != '\0') ? g_data_root_path : ".tictactoe-cx-config";
}

const char* get_config_path(void) {
    if (!g_paths_initialized) {
        (void)init_data_paths(false);
    }
    return (g_config_path[0] != '\0') ? g_config_path : "config/config.ini";
}

const char* get_highscore_path(void) {
    if (!g_paths_initialized) {
        (void)init_data_paths(false);
    }
    return (g_highscore_path[0] != '\0') ? g_highscore_path : "saves/highscores.txt";
}
