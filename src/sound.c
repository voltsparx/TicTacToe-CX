#include "sound.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
    #include <mmsystem.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <termios.h>
#endif

void sound_init(Sound* sound) {
    if (!sound) return;
    memset(sound, 0, sizeof(Sound));
    sound->enabled = true;
    sound->initialized = true;
}

void sound_close(Sound* sound) {
    (void)sound;
}

void sound_set_enabled(Sound* sound, bool enabled) {
    if (!sound) return;
    sound->enabled = enabled;
}

bool sound_is_enabled(Sound* sound) {
    return sound ? sound->enabled : false;
}

#ifdef _WIN32

static void beep_frequency(int freq, int duration) {
    if (freq <= 0 || duration <= 0) return;
    Beep(freq, duration);
}

void sound_play(Sound* sound, SoundType type) {
    if (!sound || !sound->enabled) return;
    
    switch (type) {
        case SOUND_MOVE:
            beep_frequency(800, 50);
            break;
        case SOUND_WIN:
            beep_frequency(523, 100);
            Sleep(50);
            beep_frequency(659, 100);
            Sleep(50);
            beep_frequency(784, 150);
            break;
        case SOUND_LOSE:
            beep_frequency(400, 150);
            Sleep(100);
            beep_frequency(300, 200);
            break;
        case SOUND_DRAW:
            beep_frequency(440, 80);
            Sleep(60);
            beep_frequency(370, 80);
            Sleep(60);
            beep_frequency(330, 100);
            break;
        case SOUND_UNDO:
            beep_frequency(600, 40);
            beep_frequency(400, 40);
            break;
        case SOUND_INVALID:
            beep_frequency(200, 100);
            break;
        case SOUND_TIMER:
            beep_frequency(1000, 30);
            break;
        case SOUND_ACHIEVEMENT:
            beep_frequency(523, 80);
            Sleep(40);
            beep_frequency(659, 80);
            Sleep(40);
            beep_frequency(784, 80);
            Sleep(40);
            beep_frequency(1047, 120);
            break;
        case SOUND_MENU:
            beep_frequency(440, 50);
            break;
    }
}

#else

static void play_tone_unix(int freq, int duration_ms) {
    if (freq <= 0 || duration_ms <= 0) return;
    
    int fd = open("/dev/tty", O_WRONLY);
    if (fd < 0) {
        printf("\a");
        fflush(stdout);
        return;
    }
    
    struct termios old, new;
    tcgetattr(fd, &old);
    new = old;
    new.c_lflag &= ~ICANON;
    new.c_lflag &= ~ECHO;
    tcsetattr(fd, TCSANOW, &new);
    
    write(fd, "\a", 1);
    
    usleep(duration_ms * 1000);
    
    tcsetattr(fd, TCSANOW, &old);
    close(fd);
}

static void beep_ascii(int style) {
    switch (style) {
        case 0:
            printf("\a");
            break;
        case 1:
            printf("\a");
            usleep(50000);
            printf("\a");
            break;
        case 2:
            printf("\a");
            usleep(30000);
            printf("\a");
            usleep(30000);
            printf("\a");
            break;
        case 3:
            printf("\a");
            usleep(80000);
            printf("\a");
            usleep(80000);
            printf("\a");
            usleep(80000);
            printf("\a");
            break;
    }
    fflush(stdout);
}

void sound_play(Sound* sound, SoundType type) {
    if (!sound || !sound->enabled) return;
    
    switch (type) {
        case SOUND_MOVE:
            beep_ascii(0);
            break;
        case SOUND_WIN:
            beep_ascii(3);
            break;
        case SOUND_LOSE:
            beep_ascii(1);
            usleep(150000);
            beep_ascii(1);
            break;
        case SOUND_DRAW:
            beep_ascii(2);
            break;
        case SOUND_UNDO:
            printf("\r");
            fflush(stdout);
            break;
        case SOUND_INVALID:
            beep_ascii(0);
            break;
        case SOUND_TIMER:
            beep_ascii(0);
            break;
        case SOUND_ACHIEVEMENT:
            beep_ascii(3);
            break;
        case SOUND_MENU:
            beep_ascii(0);
            break;
    }
}

#endif
