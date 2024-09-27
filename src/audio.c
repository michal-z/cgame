#include "pch.h"
#include "audio.h"

static const WAVEFORMATEX g_optimal_fmt = {
  .wFormatTag = WAVE_FORMAT_PCM,
  .nChannels = 1,
  .nSamplesPerSec = 48000,
  .nAvgBytesPerSec = 2 * 48000,
  .nBlockAlign = 2,
  .wBitsPerSample = 16,
  .cbSize = sizeof(WAVEFORMATEX),
};

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

  if (FAILED(MFStartup(MF_API_VERSION, MFSTARTUP_FULL))) {
    LOG("[audio] MFStartup() failed. Continuing without sound.");
    aud_deinit_context(aud);
    return;
  }
  LOG("[audio] Windows Media Foundation is ready.");

  if (FAILED(IXAudio2_CreateMasteringVoice(aud->engine, &aud->mastering_voice,
    XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, NULL, NULL,
    AudioCategory_GameEffects)))
  {
    LOG("[audio] Failed to create mastering voice. Continuing without sound.");
    aud_deinit_context(aud);
    return;
  }
  LOG("[audio] Mastering voice created.");

  for (uint32_t i = 0; i < AUD_MAX_SOURCE_VOICES; ++i) {
    if (FAILED(IXAudio2_CreateSourceVoice(aud->engine, &aud->source_voices[i],
      &g_optimal_fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL)))
    {
      LOG("[audio] Failed to create source voice. Continuing without sound.");
      aud_deinit_context(aud);
      return;
    }
  }
  LOG("[audio] Source voices created.");
}

void aud_deinit_context(AudContext *aud)
{
  assert(aud);
  if (aud->engine) IXAudio2_StopEngine(aud->engine);
  if (aud->mastering_voice) {
    IXAudio2MasteringVoice_DestroyVoice(aud->mastering_voice);
    aud->mastering_voice = NULL;
  }
  if (aud->engine) MFShutdown();
  SAFE_RELEASE(aud->engine);
}
