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

  u32 sampleCountAlign4 = ALIGN(audioBuffer->sampleCount, 4);
  u32 sampleCount4 = sampleCountAlign4 / 4;

  __m128 *mixerChannel0 =
      MemoryArenaPushAlignment(audioState->permanentArena, sizeof(*mixerChannel0) * sampleCount4, 16);
  __m128 *mixerChannel1 =
      MemoryArenaPushAlignment(audioState->permanentArena, sizeof(*mixerChannel1) * sampleCount4, 16);

  f32 secondsPerSample = 1.0f / (f32)audioBuffer->sampleRate;

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

    f32 *dest0 = (f32 *)mixerChannel0;
    f32 *dest1 = (f32 *)mixerChannel1;
    u32 totalSamplesToMix = audioBuffer->sampleCount;
    while (totalSamplesToMix && !isAudioFinished) {
      struct audio *loadedAudio = AudioGet(assets, playingAudio->id);
      if (loadedAudio) {
        struct audio_info *info = AudioInfoGet(assets, playingAudio->id);
        AudioPrefetch(assets, info->nextIdToPlay);

        struct v2 volume = playingAudio->currentVolume;
        struct v2 dVolume = v2_mul(playingAudio->dCurrentVolume, secondsPerSample);
        f32 dSample = playingAudio->dSample;

        assert(playingAudio->samplesPlayed >= 0.0f);

        u32 samplesToMix = totalSamplesToMix;
        f32 floatSamplesRemainingInAudio =
            (f32)(loadedAudio->sampleCount - roundf32tou32(playingAudio->samplesPlayed)) / dSample;
        u32 samplesRemainingInAudio = roundf32tou32(floatSamplesRemainingInAudio);

        if (samplesToMix > samplesRemainingInAudio) {
          samplesToMix = samplesRemainingInAudio;
        }

        b32 volumeEnded[outputChannelCount] = {};
        for (u32 channelIndex = 0; channelIndex < outputChannelCount; channelIndex++) {
          if (dVolume.e[channelIndex] != 0.0f) {
            f32 deltaVolume = playingAudio->targetVolume.e[channelIndex] - volume.e[channelIndex];
            u32 volumeSampleCount = (u32)((deltaVolume / dVolume.e[channelIndex]) + 0.5f);
            if (samplesToMix > volumeSampleCount) {
              samplesToMix = volumeSampleCount;
              volumeEnded[channelIndex] = 1;
            }
          }
        }

        // TODO(e2dk4r): handle stereo
        f32 samplePosition = playingAudio->samplesPlayed;
        for (u32 loopIndex = 0; loopIndex < samplesToMix; loopIndex++) {
#if 1
          // linear interplation
          s32 sampleIndex = Floor(samplePosition);
          f32 frac = samplePosition - (f32)sampleIndex;

          f32 sample0 = (f32)loadedAudio->samples[0][sampleIndex];
          f32 sample1 = (f32)loadedAudio->samples[0][sampleIndex + 1];

          f32 sampleValue = Lerp(sample0, sample1, frac);
#else
          u32 sampleIndex = roundf32tou32(samplePosition);
          f32 sampleValue = (f32)loadedAudio->samples[0][sampleIndex];
#endif

          *dest0++ += (audioState->masterVolume.e[0] * volume.e[0]) * sampleValue;
          *dest1++ += (audioState->masterVolume.e[0] * volume.e[1]) * sampleValue;

          v2_add_ref(&volume, dVolume);
          samplePosition += dSample;
        }

        playingAudio->currentVolume = volume;

        for (u32 channelIndex = 0; channelIndex < outputChannelCount; channelIndex++) {
          if (volumeEnded[channelIndex]) {
            playingAudio->currentVolume.e[channelIndex] = playingAudio->targetVolume.e[channelIndex];
            playingAudio->dCurrentVolume.e[channelIndex] = 0.0f;
          }
        }

        playingAudio->samplesPlayed = samplePosition;

        assert(totalSamplesToMix >= samplesToMix);
        totalSamplesToMix -= samplesToMix;

        isWritten = 1;
        if ((u32)playingAudio->samplesPlayed == loadedAudio->sampleCount) {
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
