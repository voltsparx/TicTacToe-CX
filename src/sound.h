#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>

typedef enum {
    SOUND_MOVE,
    SOUND_WIN,
    SOUND_LOSE,
    SOUND_DRAW,
    SOUND_UNDO,
    SOUND_INVALID,
    SOUND_TIMER,
    SOUND_ACHIEVEMENT,
    SOUND_MENU
} SoundType;

typedef struct {
    bool enabled;
    bool initialized;
} Sound;

void sound_init(Sound* sound);
void sound_close(Sound* sound);
void sound_play(Sound* sound, SoundType type);
void sound_set_enabled(Sound* sound, bool enabled);
bool sound_is_enabled(Sound* sound);

#endif
