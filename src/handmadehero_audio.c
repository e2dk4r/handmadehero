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

  assert(IS_ALIGNED(audioBuffer->sampleCount, 8));
  u32 sampleCount8 = audioBuffer->sampleCount / 8;
  u32 sampleCount4 = audioBuffer->sampleCount / 4;

  __m128 *mixerChannel0 =
      MemoryArenaPushAlignment(audioState->permanentArena, sizeof(*mixerChannel0) * sampleCount4, 16);
  __m128 *mixerChannel1 =
      MemoryArenaPushAlignment(audioState->permanentArena, sizeof(*mixerChannel1) * sampleCount4, 16);

  f32 secondsPerSample = 1.0f / (f32)audioBuffer->sampleRate;

  __m128 masterVolume4_0 = _mm_set1_ps(audioState->masterVolume.e[0]);
  __m128 masterVolume4_1 = _mm_set1_ps(audioState->masterVolume.e[1]);

  enum { outputChannelCount = 2 };

  // clear out mixer channels
  __m128 zero = _mm_set1_ps(0.0f);
  {
    __m128 *dest0 = mixerChannel0;
    __m128 *dest1 = mixerChannel1;
    for (u32 sampleIndex = 0; sampleIndex < sampleCount4; sampleIndex++) {
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
    u32 totalSamplesToMix8 = audioBuffer->sampleCount / 8;
    while (totalSamplesToMix8 && !isAudioFinished) {
      struct audio *loadedAudio = AudioGet(assets, playingAudio->id);
      if (loadedAudio) {
        struct audio_info *info = AudioInfoGet(assets, playingAudio->id);
        AudioPrefetch(assets, info->nextIdToPlay);

        struct v2 volume = playingAudio->currentVolume;
        struct v2 dVolume = v2_mul(playingAudio->dCurrentVolume, secondsPerSample);
        struct v2 dVolume8 = v2_mul(v2_mul(playingAudio->dCurrentVolume, secondsPerSample), 8.0f);
        f32 dSample = playingAudio->dSample;
        f32 dSample8 = dSample * 8.0f;

        __m128 volume4_0 = _mm_setr_ps(volume.e[0] + 0.0f * dVolume.e[0], volume.e[0] + 1.0f * dVolume.e[0],
                                       volume.e[0] + 2.0f * dVolume.e[0], volume.e[0] + 3.0f * dVolume.e[0]);
        __m128 volume4_1 = _mm_setr_ps(volume.e[1] + 0.0f * dVolume.e[1], volume.e[1] + 1.0f * dVolume.e[1],
                                       volume.e[1] + 2.0f * dVolume.e[1], volume.e[1] + 3.0f * dVolume.e[1]);

        __m128 dVolume4_0 = _mm_set1_ps(dVolume.e[0]);
        __m128 dVolume4_1 = _mm_set1_ps(dVolume.e[1]);
        __m128 dVolume84_0 = _mm_set1_ps(dVolume8.e[0]);
        __m128 dVolume84_1 = _mm_set1_ps(dVolume8.e[1]);

        assert(playingAudio->samplesPlayed >= 0.0f);

        u32 samplesToMix8 = totalSamplesToMix8;
        f32 floatSamplesRemainingInAudio8 =
            (f32)(loadedAudio->sampleCount - roundf32tou32(playingAudio->samplesPlayed)) / dSample8;
        u32 samplesRemainingInAudio8 = roundf32tou32(floatSamplesRemainingInAudio8);

        if (samplesToMix8 > samplesRemainingInAudio8) {
          samplesToMix8 = samplesRemainingInAudio8;
        }

        b32 volumeEnded[outputChannelCount] = {};
        for (u32 channelIndex = 0; channelIndex < outputChannelCount; channelIndex++) {
          if (dVolume8.e[channelIndex] != 0.0f) {
            f32 deltaVolume = playingAudio->targetVolume.e[channelIndex] - volume.e[channelIndex];
            u32 volumeSampleCount8 = (u32)((1.0f / 8.0f) * ((deltaVolume / dVolume8.e[channelIndex]) + 0.5f));
            if (samplesToMix8 > volumeSampleCount8) {
              samplesToMix8 = volumeSampleCount8;
              volumeEnded[channelIndex] = 1;
            }
          }
        }

        // TODO(e2dk4r): handle stereo
        f32 samplePosition = playingAudio->samplesPlayed;
        for (u32 loopIndex = 0; loopIndex < samplesToMix8; loopIndex++) {
#if 0
          // linear interplation
          f32 offsetSamplePosition = samplePosition;
          u32 sampleIndex = (u32)Floor(offsetSamplePosition);
          f32 frac = offsetSamplePosition - (f32)sampleIndex;

          f32 sample0 = (f32)loadedAudio->samples[0][sampleIndex];
          f32 sample1 = (f32)loadedAudio->samples[0][sampleIndex + 1];

          f32 sampleValue = Lerp(sample0, sample1, frac);
#else
          __m128 sampleValue_0 = _mm_setr_ps(loadedAudio->samples[0][roundf32tou32(samplePosition + 0.0f * dSample)],
                                             loadedAudio->samples[0][roundf32tou32(samplePosition + 1.0f * dSample)],
                                             loadedAudio->samples[0][roundf32tou32(samplePosition + 2.0f * dSample)],
                                             loadedAudio->samples[0][roundf32tou32(samplePosition + 3.0f * dSample)]);
          __m128 sampleValue_1 = _mm_setr_ps(loadedAudio->samples[0][roundf32tou32(samplePosition + 4.0f * dSample)],
                                             loadedAudio->samples[0][roundf32tou32(samplePosition + 5.0f * dSample)],
                                             loadedAudio->samples[0][roundf32tou32(samplePosition + 6.0f * dSample)],
                                             loadedAudio->samples[0][roundf32tou32(samplePosition + 7.0f * dSample)]);

#endif
          __m128 d0_0 = _mm_load_ps((f32 *)&dest0[0]);
          __m128 d0_1 = _mm_load_ps((f32 *)&dest0[1]);
          __m128 d1_0 = _mm_load_ps((f32 *)&dest1[0]);
          __m128 d1_1 = _mm_load_ps((f32 *)&dest1[1]);

          d0_0 = _mm_add_ps(d0_0, _mm_mul_ps(_mm_mul_ps(masterVolume4_0, volume4_0), sampleValue_0));
          d0_1 = _mm_add_ps(d0_1,
                            _mm_mul_ps(_mm_mul_ps(masterVolume4_0, _mm_add_ps(volume4_0, dVolume4_0)), sampleValue_1));
          d1_0 = _mm_add_ps(d1_0, _mm_mul_ps(_mm_mul_ps(masterVolume4_1, volume4_1), sampleValue_0));
          d1_1 = _mm_add_ps(d1_1,
                            _mm_mul_ps(_mm_mul_ps(masterVolume4_1, _mm_add_ps(volume4_1, dVolume4_1)), sampleValue_1));

          _mm_store_ps((f32 *)&dest0[0], d0_0);
          _mm_store_ps((f32 *)&dest0[1], d0_1);
          _mm_store_ps((f32 *)&dest1[0], d1_0);
          _mm_store_ps((f32 *)&dest1[1], d1_1);

          dest0 += 2;
          dest1 += 2;

          volume4_0 = _mm_add_ps(volume4_0, dVolume84_0);
          volume4_1 = _mm_add_ps(volume4_1, dVolume84_1);
          v2_add_ref(&volume, dVolume8);
          samplePosition += dSample8;
        }

        playingAudio->currentVolume = volume;

        for (u32 channelIndex = 0; channelIndex < outputChannelCount; channelIndex++) {
          if (volumeEnded[channelIndex]) {
            playingAudio->currentVolume.e[channelIndex] = playingAudio->targetVolume.e[channelIndex];
            playingAudio->dCurrentVolume.e[channelIndex] = 0.0f;
          }
        }

        playingAudio->samplesPlayed = samplePosition;

        assert(totalSamplesToMix8 >= samplesToMix8);
        totalSamplesToMix8 -= samplesToMix8;

        isWritten = 1;
        if ((u32)playingAudio->samplesPlayed >= loadedAudio->sampleCount) {
          if (IsAudioIdValid(info->nextIdToPlay)) {
            playingAudio->id = info->nextIdToPlay;
            playingAudio->samplesPlayed = 0;
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
    for (u32 sampleIndex = 0; sampleIndex < sampleCount4; sampleIndex++) {
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
