#include <handmadehero/audio.h>
#include <handmadehero/memory_arena.h>
#include <x86intrin.h>

void
AudioStateInit(struct audio_state *audioState, struct memory_arena *permanentArena)
{
  audioState->permanentArena = permanentArena;
  audioState->firstPlayingAudio = 0;
  audioState->firstFreePlayingAudio = 0;
  audioState->masterVolume = v2(1.0f, 1.0f);
}

b32
OutputPlayingAudios(struct audio_state *audioState, struct game_audio_buffer *audioBuffer, struct game_assets *assets)
{
  b32 isWritten = 0;
  struct memory_temp mixerMemory = BeginTemporaryMemory(audioState->permanentArena);

  assert(IS_ALIGNED(audioBuffer->sampleCount, 4));
  u32 chunkCount = audioBuffer->sampleCount / 4;

  __m128 *mixerChannel0 = MemoryArenaPushAlignment(audioState->permanentArena, sizeof(*mixerChannel0) * chunkCount, 16);
  __m128 *mixerChannel1 = MemoryArenaPushAlignment(audioState->permanentArena, sizeof(*mixerChannel1) * chunkCount, 16);

  f32 secondsPerSample = 1.0f / (f32)audioBuffer->sampleRate;

  __m128 masterVolume0 = _mm_set1_ps(audioState->masterVolume.e[0]);
  __m128 masterVolume1 = _mm_set1_ps(audioState->masterVolume.e[1]);

  enum { outputChannelCount = 2 };

  // clear out mixer channels
  __m128 zero = _mm_set1_ps(0.0f);
  {
    __m128 *dest0 = mixerChannel0;
    __m128 *dest1 = mixerChannel1;
    for (u32 sampleIndex = 0; sampleIndex < chunkCount; sampleIndex++) {
      _mm_store_ps((f32 *)dest0++, zero);
      _mm_store_ps((f32 *)dest1++, zero);
    }
  }

  // sum all audios to mixer channels
  for (struct playing_audio **playingAudioPtr = &audioState->firstPlayingAudio; *playingAudioPtr;) {
    struct playing_audio *playingAudio = *playingAudioPtr;
    b32 isAudioFinished = 0;

    __m128 *dest0 = mixerChannel0;
    __m128 *dest1 = mixerChannel1;
    u32 totalChunksToMix = chunkCount;
    while (totalChunksToMix && !isAudioFinished) {
      struct audio *loadedAudio = AudioGet(assets, playingAudio->id);
      if (loadedAudio) {
        struct audio_info *info = AudioInfoGet(assets, playingAudio->id);
        AudioPrefetch(assets, info->nextIdToPlay);

        struct v2 volume = playingAudio->currentVolume;
        struct v2 dVolume = v2_mul(playingAudio->dCurrentVolume, secondsPerSample);
        struct v2 dVolumeChunk = v2_mul(v2_mul(playingAudio->dCurrentVolume, secondsPerSample), 4.0f);
        f32 dSample = playingAudio->dSample;
        f32 dSampleChunk = dSample * 4.0f;

        // channel 0
        __m128 volume0 = _mm_setr_ps(volume.e[0] + 0.0f * dVolume.e[0], volume.e[0] + 1.0f * dVolume.e[0],
                                     volume.e[0] + 2.0f * dVolume.e[0], volume.e[0] + 3.0f * dVolume.e[0]);
        __m128 dVolume0 = _mm_set1_ps(dVolume.e[0]);
        __m128 dVolumeChunk0 = _mm_set1_ps(dVolumeChunk.e[0]);

        // channel 1
        __m128 volume1 = _mm_setr_ps(volume.e[1] + 0.0f * dVolume.e[1], volume.e[1] + 1.0f * dVolume.e[1],
                                     volume.e[1] + 2.0f * dVolume.e[1], volume.e[1] + 3.0f * dVolume.e[1]);
        __m128 dVolume1 = _mm_set1_ps(dVolume.e[1]);
        __m128 dVolumeChunk1 = _mm_set1_ps(dVolumeChunk.e[1]);

        assert(playingAudio->samplesPlayed >= 0.0f);

        u32 chunksToMix = totalChunksToMix;
        f32 floatChunksRemainingInAudio =
            (f32)(loadedAudio->sampleCount - roundf32tou32(playingAudio->samplesPlayed)) / dSampleChunk;
        u32 chunksRemainingInAudio = roundf32tou32(floatChunksRemainingInAudio);

        if (chunksToMix > chunksRemainingInAudio) {
          chunksToMix = chunksRemainingInAudio;
        }

        b32 volumeEnded[outputChannelCount] = {};
        for (u32 channelIndex = 0; channelIndex < outputChannelCount; channelIndex++) {
          if (dVolumeChunk.e[channelIndex] != 0.0f) {
            f32 deltaVolume = playingAudio->targetVolume.e[channelIndex] - volume.e[channelIndex];
            u32 volumeChunkCount = (u32)((1.0f / 8.0f) * ((deltaVolume / dVolumeChunk.e[channelIndex]) + 0.5f));
            if (chunksToMix > volumeChunkCount) {
              chunksToMix = volumeChunkCount;
              volumeEnded[channelIndex] = 1;
            }
          }
        }

        // TODO(e2dk4r): handle stereo
        f32 samplePosition = playingAudio->samplesPlayed;
        for (u32 loopIndex = 0; loopIndex < chunksToMix; loopIndex++) {
#if 0
          // linear interplation
          f32 offsetSamplePosition = samplePosition;
          u32 sampleIndex = (u32)Floor(offsetSamplePosition);
          f32 frac = offsetSamplePosition - (f32)sampleIndex;

          f32 sample0 = (f32)loadedAudio->samples[0][sampleIndex];
          f32 sample1 = (f32)loadedAudio->samples[0][sampleIndex + 1];

          f32 sampleValue = Lerp(sample0, sample1, frac);
#else
          __m128 sampleValue = _mm_setr_ps(loadedAudio->samples[0][roundf32tou32(samplePosition + 0.0f * dSample)],
                                           loadedAudio->samples[0][roundf32tou32(samplePosition + 1.0f * dSample)],
                                           loadedAudio->samples[0][roundf32tou32(samplePosition + 2.0f * dSample)],
                                           loadedAudio->samples[0][roundf32tou32(samplePosition + 3.0f * dSample)]);

#endif
          __m128 d0 = _mm_load_ps((f32 *)&dest0[0]);
          __m128 d1 = _mm_load_ps((f32 *)&dest1[0]);

          d0 = _mm_add_ps(d0, _mm_mul_ps(_mm_mul_ps(masterVolume0, volume0), sampleValue));
          d1 = _mm_add_ps(d1, _mm_mul_ps(_mm_mul_ps(masterVolume1, volume1), sampleValue));

          _mm_store_ps((f32 *)&dest0[0], d0);
          _mm_store_ps((f32 *)&dest1[0], d1);

          dest0++;
          dest1++;

          volume0 = _mm_add_ps(volume0, dVolumeChunk0);
          volume1 = _mm_add_ps(volume1, dVolumeChunk1);
          v2_add_ref(&volume, dVolumeChunk);
          samplePosition += dSampleChunk;
        }

        playingAudio->currentVolume = volume;

        for (u32 channelIndex = 0; channelIndex < outputChannelCount; channelIndex++) {
          if (volumeEnded[channelIndex]) {
            playingAudio->currentVolume.e[channelIndex] = playingAudio->targetVolume.e[channelIndex];
            playingAudio->dCurrentVolume.e[channelIndex] = 0.0f;
          }
        }

        playingAudio->samplesPlayed = samplePosition;

        assert(totalChunksToMix >= chunksToMix);
        totalChunksToMix -= chunksToMix;

        isWritten = 1;
        if ((u32)playingAudio->samplesPlayed >= loadedAudio->sampleCount) {
          if (IsAudioIdValid(info->nextIdToPlay)) {
            playingAudio->id = info->nextIdToPlay;
            playingAudio->samplesPlayed -= (f32)loadedAudio->sampleCount;
          } else {
            isAudioFinished = 1;
            break;
          }
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
    __m128 *source0 = mixerChannel0;
    __m128 *source1 = mixerChannel1;

    __m128i *sampleOut = (__m128i *)audioBuffer->samples;
    for (u32 sampleIndex = 0; sampleIndex < chunkCount; sampleIndex++) {
      __m128i l = _mm_cvtps_epi32(*source0++);
      __m128i r = _mm_cvtps_epi32(*source1++);

      __m128i lr0 = _mm_unpacklo_epi32(l, r);
      __m128i lr1 = _mm_unpackhi_epi32(l, r);

      __m128i s01 = _mm_packs_epi32(lr0, lr1);

      *sampleOut++ = s01;
    }
  }

  EndTemporaryMemory(&mixerMemory);
  return isWritten;
}

struct playing_audio *
PlayAudio(struct audio_state *audioState, struct audio_id id)
{
  if (!audioState->firstFreePlayingAudio) {
    audioState->firstFreePlayingAudio =
        MemoryArenaPush(audioState->permanentArena, sizeof(*audioState->firstPlayingAudio));
    audioState->firstFreePlayingAudio->next = 0;
  }

  struct playing_audio *playingAudio = audioState->firstFreePlayingAudio;
  audioState->firstFreePlayingAudio = playingAudio->next;
  playingAudio->samplesPlayed = 0;
  playingAudio->currentVolume = playingAudio->targetVolume = v2(1.0f, 1.0f);
  playingAudio->dCurrentVolume = v2(0.0f, 0.0f);
  playingAudio->dSample = 1.0f;
  playingAudio->id = id;

  playingAudio->next = audioState->firstPlayingAudio;
  audioState->firstPlayingAudio = playingAudio;

  return playingAudio;
}

void
ChangeVolume(struct audio_state *audioState, struct playing_audio *playingAudio, f32 fadeDurationInSeconds,
             struct v2 volume)
{
  if (fadeDurationInSeconds <= 0.0f) {
    playingAudio->currentVolume = playingAudio->targetVolume = volume;
  } else {
    playingAudio->targetVolume = volume;
    playingAudio->dCurrentVolume =
        v2_mul(v2_sub(playingAudio->targetVolume, playingAudio->currentVolume), 1.0f / fadeDurationInSeconds);
  }
}

void
ChangePitch(struct audio_state *audioState, struct playing_audio *playingAudio, f32 dSample)
{
  playingAudio->dSample = dSample;
}
