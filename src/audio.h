#pragma once

typedef struct AudSound
{
  alignas(4) uint16_t index;
  uint16_t generation;
} AudSound;

static_assert(alignof(AudSound) == 4);

typedef struct AudSourceVoiceArray
{
  IXAudio2SourceVoice **items;
} AudSourceVoiceArray;

typedef struct AudContext
{
  IXAudio2 *engine;
  IXAudio2MasteringVoice *mastering_voice;
  AudSourceVoiceArray source_voices;
  struct SoundPool *sound_pool;
} AudContext;

void aud_init_context(AudContext *aud);
void aud_deinit_context(AudContext *aud);

IXAudio2SourceVoice *aud_find_idle_source_voice(AudContext *aud);
