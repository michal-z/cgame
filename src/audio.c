#include "pch.h"
#include "audio.h"

void aud_init_context(AudContext *aud)
{
  assert(aud && aud->engine == NULL);

  if (GetModuleHandle(DX12_SDK_PATH XAUDIO2_DLL_A) == NULL) {
    HMODULE m = LoadLibrary(DX12_SDK_PATH XAUDIO2_DLL_A);
    if (m == NULL) return;

    LOG("[audio] Loaded XAudio2 DLL");
  }
}

void aud_deinit_context(AudContext *aud)
{
  assert(aud);
  SAFE_RELEASE(aud->engine);
}
