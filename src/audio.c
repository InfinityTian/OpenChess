#include "audio.h"

#include <stdio.h>
#include <string.h>

#if HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif

static const char *SND_FILES[SND_COUNT] = {
    "move-self.mp3", "move-opponent.mp3", "capture.mp3", "castle.mp3",
    "move-check.mp3", "promote.mp3", "illegal.mp3", "notify.mp3",
    "game-end.mp3", "game-win.mp3", "game-lose.mp3", "game-draw.mp3"
};

#if HAVE_SDL_MIXER

static Mix_Chunk *s_chunks[SND_COUNT];
static bool s_ok = false;
static bool s_enabled = true;

void audio_init(const char *dir)
{
    if (s_ok) return;
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
        fprintf(stderr, "SDL_mixer: %s\n", Mix_GetError());
        return;
    }
    Mix_AllocateChannels(8);

    int loaded = 0;
    for (int i = 0; i < SND_COUNT; i++) {
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", dir ? dir : "", SND_FILES[i]);
        s_chunks[i] = Mix_LoadWAV(path);
        if (s_chunks[i]) loaded++;
    }
    if (loaded == 0) {
        Mix_CloseAudio();
        return;
    }
    s_ok = true;
}

void audio_shutdown(void)
{
    if (!s_ok) return;
    for (int i = 0; i < SND_COUNT; i++) {
        if (s_chunks[i]) Mix_FreeChunk(s_chunks[i]);
        s_chunks[i] = NULL;
    }
    Mix_CloseAudio();
    s_ok = false;
}

bool audio_available(void) { return s_ok; }
void audio_set_enabled(bool on) { s_enabled = on; }
bool audio_enabled(void) { return s_enabled; }

void audio_play(SoundId id)
{
    if (!s_ok || !s_enabled) return;
    if (id < 0 || id >= SND_COUNT || !s_chunks[id]) return;
    Mix_PlayChannel(-1, s_chunks[id], 0);
}

#else  /* !HAVE_SDL_MIXER */

void audio_init(const char *dir) { (void)dir; }
void audio_shutdown(void) {}
bool audio_available(void) { return false; }
void audio_set_enabled(bool on) { (void)on; }
bool audio_enabled(void) { return false; }
void audio_play(SoundId id) { (void)id; }

#endif
