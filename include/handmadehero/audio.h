#ifndef HANDMADEHERO_AUDIO_H
#define HANDMADEHERO_AUDIO_H

#include "asset.h"
#include "types.h"

struct playing_audio {
  struct audio_id id;
  f32 volume[2];
  u32 samplesPlayed;
  struct playing_audio *next;
};

struct game_audio_state {
  struct memory_arena *permanentArena;
  struct playing_audio *firstPlayingAudio;
  struct playing_audio *firstFreePlayingAudio;
};

void
AudioStateInit(struct game_audio_state *audioState, struct memory_arena *permanentArena);

b32
OutputPlayingAudios(struct game_audio_state *audioState, struct game_audio_buffer *audioBuffer,
                    struct game_assets *assets);

struct playing_audio *
PlayAudio(struct game_audio_state *audioState, struct audio_id id);

#endif /* HANDMADEHERO_AUDIO_H */
