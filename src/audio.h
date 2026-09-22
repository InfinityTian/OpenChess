#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

/*
 * Tiny sound layer. Sounds are the chess.com "default" set (see
 * scripts/import_sounds.sh). Built only when SDL2_mixer is available
 * (HAVE_SDL_MIXER); every call is a no-op otherwise.
 */
typedef enum {
    SND_MOVE_SELF = 0,
    SND_MOVE_OPPONENT,
    SND_CAPTURE,
    SND_CASTLE,
    SND_CHECK,
    SND_PROMOTE,
    SND_ILLEGAL,
    SND_NOTIFY,
    SND_GAME_END,
    SND_GAME_WIN,
    SND_GAME_LOSE,
    SND_GAME_DRAW,
    SND_COUNT
} SoundId;

/* Open the audio device and load every sound from `dir`. */
void audio_init(const char *dir);
void audio_shutdown(void);

/* True when the mixer initialised and the sounds loaded. */
bool audio_available(void);

void audio_set_enabled(bool on);
bool audio_enabled(void);

void audio_play(SoundId id);

#endif
