#pragma once

typedef struct AudContext
{
  IXAudio2 *engine;
} AudContext;

void aud_init_context(AudContext *aud);
void aud_deinit_context(AudContext *aud);
