#include "pch.h"
#include "audio.h"

void aud_init_context(AudContext *aud)
{
  assert(aud && aud->engine == NULL);

  HMODULE xaudio2_dll = GetModuleHandle(DX12_SDK_PATH XAUDIO2_DLL_A);
  if (xaudio2_dll == NULL) {
    xaudio2_dll = LoadLibrary(DX12_SDK_PATH XAUDIO2_DLL_A);
    if (xaudio2_dll == NULL) {
      LOG("[audio] Failed to load XAudio2 DLL. Continuing without sound.");
      return;
    }
    LOG("[audio] Loaded XAudio2 DLL");
  }
  assert(xaudio2_dll);

  typedef HRESULT (__stdcall *XAudio2CreateFn)(IXAudio2 **, UINT32,
    XAUDIO2_PROCESSOR);

#pragma warning(push)
#pragma warning(disable : 4191)
  XAudio2CreateFn xaudio2_create = (XAudio2CreateFn)GetProcAddress(xaudio2_dll,
    "XAudio2Create");
#pragma warning(pop)
  assert(xaudio2_create);

  if (FAILED(xaudio2_create(&aud->engine, 0, XAUDIO2_USE_DEFAULT_PROCESSOR))) {
    LOG("[audio] Failed to create XAudio2 engine. Continuing without sound.");
    return;
  }
  LOG("[audio] XAudio2 engine created");
}

void aud_deinit_context(AudContext *aud)
{
  assert(aud);
  SAFE_RELEASE(aud->engine);
}
