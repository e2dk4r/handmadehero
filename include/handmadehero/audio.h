#ifndef HANDMADEHERO_AUDIO_H
#define HANDMADEHERO_AUDIO_H

#include "asset.h"
#include "types.h"

struct playing_audio {
  struct v2 currentVolume;
  struct v2 dCurrentVolume;
  struct v2 targetVolume;

  struct audio_id id;
  f32 samplesPlayed;
  struct playing_audio *next;
};

struct audio_state {
  struct memory_arena *permanentArena;
  struct playing_audio *firstPlayingAudio;
  struct playing_audio *firstFreePlayingAudio;

  struct v2 masterVolume;
};

void
AudioStateInit(struct audio_state *audioState, struct memory_arena *permanentArena);

b32
OutputPlayingAudios(struct audio_state *audioState, struct game_audio_buffer *audioBuffer, struct game_assets *assets);

struct playing_audio *
PlayAudio(struct audio_state *audioState, struct audio_id id);

void
ChangeVolume(struct audio_state *audioState, struct playing_audio *playingAudio, f32 fadeDurationInSeconds,
             struct v2 volume);

#endif /* HANDMADEHERO_AUDIO_H */
