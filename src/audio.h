#pragma once

typedef struct AudSound
{
  alignas(4) uint16_t index;
  uint16_t generation;
} AudSound;

static_assert(sizeof(AudSound) == 4 && alignof(AudSound) == 4);

typedef struct AudSourceVoiceArray
{
  IXAudio2SourceVoice **items;
} AudSourceVoiceArray;

typedef struct AudPlaySoundArgs
{
  uint32_t play_begin;
  uint32_t play_length;
  uint32_t loop_begin;
  uint32_t loop_length;
  uint32_t loop_count;
} AudPlaySoundArgs;

typedef struct AudContext
{
  IXAudio2 *engine;
  IXAudio2MasteringVoice *mastering_voice;
  AudSourceVoiceArray source_voices;
  struct SoundPool *sound_pool;
} AudContext;

void aud_init_context(AudContext *aud);
void aud_deinit_context(AudContext *aud);
AudSound aud_create_sound_from_file(AudContext *aud, const char *filename);
void aud_destroy_sound(AudContext *aud, AudSound sound);
bool aud_is_sound_valid(AudContext *aud, AudSound sound);
void aud_play_sound(AudContext *aud, AudSound sound, AudPlaySoundArgs *args);

IXAudio2SourceVoice *aud_find_idle_source_voice(AudContext *aud);
