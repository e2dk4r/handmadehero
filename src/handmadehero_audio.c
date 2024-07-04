#include <handmadehero/audio.h>
#include <handmadehero/memory_arena.h>

void
AudioStateInit(struct audio_state *audioState, struct memory_arena *permanentArena)
{
  audioState->permanentArena = permanentArena;
  audioState->firstPlayingAudio = 0;
  audioState->firstFreePlayingAudio = 0;
}

b32
OutputPlayingAudios(struct audio_state *audioState, struct game_audio_buffer *audioBuffer, struct game_assets *assets)
{
  b32 isWritten = 0;
  struct memory_temp mixerMemory = BeginTemporaryMemory(audioState->permanentArena);

  f32 *mixerChannel0 = MemoryArenaPush(audioState->permanentArena, sizeof(*mixerChannel0) * audioBuffer->sampleCount);
  f32 *mixerChannel1 = MemoryArenaPush(audioState->permanentArena, sizeof(*mixerChannel1) * audioBuffer->sampleCount);

  f32 secondsPerSample = 1.0f / (f32)audioBuffer->sampleRate;

  enum { outputChannelCount = 2 };

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

        struct v2 volume = playingAudio->currentVolume;
        struct v2 dVolume = v2_mul(playingAudio->dCurrentVolume, secondsPerSample);

        u32 samplesToMix = totalSamplesToMix;
        u32 samplesRemainingInAudio = loadedAudio->sampleCount - playingAudio->samplesPlayed;

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
        for (u32 sampleIndex = playingAudio->samplesPlayed; sampleIndex < playingAudio->samplesPlayed + samplesToMix;
             sampleIndex++) {
          f32 sampleValue = (f32)loadedAudio->samples[0][sampleIndex];
          *dest0++ += volume.e[0] * sampleValue;
          *dest1++ += volume.e[1] * sampleValue;

          v2_add_ref(&volume, dVolume);
        }

        playingAudio->currentVolume = volume;

        for (u32 channelIndex = 0; channelIndex < outputChannelCount; channelIndex++) {
          if (volumeEnded[channelIndex]) {
            playingAudio->currentVolume.e[channelIndex] = playingAudio->targetVolume.e[channelIndex];
            playingAudio->dCurrentVolume.e[channelIndex] = 0.0f;
          }
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
