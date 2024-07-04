#include <handmadehero/audio.h>
#include <handmadehero/handmadehero.h>

b32
OutputPlayingAudios(struct game_audio_state *audioState, struct game_audio_buffer *audioBuffer,
                    struct game_assets *assets, struct memory_arena *metaArena)
{
  b32 isWritten = 0;
  struct memory_temp mixerMemory = BeginTemporaryMemory(metaArena);

  f32 *mixerChannel0 = MemoryArenaPush(metaArena, sizeof(*mixerChannel0) * audioBuffer->sampleCount);
  f32 *mixerChannel1 = MemoryArenaPush(metaArena, sizeof(*mixerChannel1) * audioBuffer->sampleCount);

  // clear out mixer channels
  {
    f32 *dest0 = mixerChannel0;
    f32 *dest1 = mixerChannel1;
    for (u32 sampleIndex = 0; sampleIndex < audioBuffer->sampleCount; sampleIndex++) {
      *dest0++ = 0.0f;
      *dest1++ = 0.0f;
    }
  }

  // sum all audios to mixer channels
  for (struct playing_audio **playingAudioPtr = &audioState->firstPlayingAudio; *playingAudioPtr;) {
    struct playing_audio *playingAudio = *playingAudioPtr;
    b32 isAudioFinished = 0;

    f32 *dest0 = mixerChannel0;
    f32 *dest1 = mixerChannel1;
    u32 totalSamplesToMix = audioBuffer->sampleCount;
    while (totalSamplesToMix && !isAudioFinished) {
      struct audio *loadedAudio = AudioGet(assets, playingAudio->id);
      if (loadedAudio) {
        struct audio_info *info = AudioInfoGet(assets, playingAudio->id);
        AudioPrefetch(assets, info->nextIdToPlay);

        // TODO(e2dk4r): handle stereo
        f32 volume0 = playingAudio->volume[0];
        f32 volume1 = playingAudio->volume[1];

        u32 samplesToMix = totalSamplesToMix;
        u32 samplesRemainingInAudio = loadedAudio->sampleCount - playingAudio->samplesPlayed;
        if (samplesToMix > samplesRemainingInAudio) {
          samplesToMix = samplesRemainingInAudio;
        }

        for (u32 sampleIndex = playingAudio->samplesPlayed; sampleIndex < playingAudio->samplesPlayed + samplesToMix;
             sampleIndex++) {
          f32 sampleValue = (f32)loadedAudio->samples[0][sampleIndex];
          *dest0++ += volume0 * sampleValue;
          *dest1++ += volume1 * sampleValue;
        }
        playingAudio->samplesPlayed += samplesToMix;

        assert(totalSamplesToMix >= samplesToMix);
        totalSamplesToMix -= samplesToMix;

        isWritten = 1;
        if (playingAudio->samplesPlayed == loadedAudio->sampleCount) {
          if (IsAudioIdValid(info->nextIdToPlay)) {
            playingAudio->id = info->nextIdToPlay;
            playingAudio->samplesPlayed = 0;
          } else {
            isAudioFinished = 1;
            break;
          }
        } else {
          assert(totalSamplesToMix == 0);
        }
      } else {
        // audio is not in cache
        AudioLoad(assets, playingAudio->id);
        break;
      }
    }

    if (isAudioFinished) {
      *playingAudioPtr = playingAudio->next;
      playingAudio->next = audioState->firstFreePlayingAudio;
      audioState->firstFreePlayingAudio = playingAudio;
    } else {
      playingAudioPtr = &playingAudio->next;
    }
  }

  // convert to 16-bit
  if (isWritten) {
    f32 *source0 = mixerChannel0;
    f32 *source1 = mixerChannel1;

    s16 *sampleOut = audioBuffer->samples;
    for (u32 sampleIndex = 0; sampleIndex < audioBuffer->sampleCount; sampleIndex++) {
      *sampleOut++ = (s16)(*source0++ + 0.5f);
      *sampleOut++ = (s16)(*source1++ + 0.5f);
    }
  }

  EndTemporaryMemory(&mixerMemory);
  return isWritten;
}
