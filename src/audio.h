#pragma once

#define AUD_MAX_SOURCE_VOICES 32

typedef struct AudContext
{
  IXAudio2 *engine;
  IXAudio2MasteringVoice *mastering_voice;
  IXAudio2SourceVoice *source_voices[AUD_MAX_SOURCE_VOICES];
} AudContext;

void aud_init_context(AudContext *aud);
void aud_deinit_context(AudContext *aud);
