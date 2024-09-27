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

static uint8_t *
decode_audio_from_file(const char *filename)
{
  wchar_t filename_w[MAX_PATH];
  mbstowcs_s(NULL, filename_w, MAX_PATH, filename, MAX_PATH - 1);

  IMFSourceReader *src_reader;
  if (FAILED(MFCreateSourceReaderFromURL(filename_w, NULL, &src_reader))) {
    LOG("Failed to decode audio file (%s).", filename);
    return NULL;
  }

  IMFMediaType *media_type = NULL;
  VHR(IMFSourceReader_GetNativeMediaType(src_reader,
    (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &media_type));

  VHR(IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio));
  VHR(IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM));
  VHR(IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_NUM_CHANNELS,
    g_optimal_fmt.nChannels));
  VHR(IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND,
    g_optimal_fmt.nSamplesPerSec));
}

//
// Pooled source voices (Psv) callback functions
//
static void
Psv_OnVoiceProcessingPassStart(IXAudio2VoiceCallback *self, UINT32 BytesRequired)
{
  (void)self; (void)BytesRequired;
}

static void
Psv_OnVoiceProcessingPassEnd(IXAudio2VoiceCallback *self)
{
  (void)self;
}

static void
Psv_OnStreamEnd(IXAudio2VoiceCallback *self)
{
  (void)self;
}

static void
Psv_OnBufferStart(IXAudio2VoiceCallback *self, void *buffer_ctx)
{
  (void)self; (void)buffer_ctx;
}

static void
Psv_OnBufferEnd(IXAudio2VoiceCallback *self, void *buffer_ctx)
{
  (void)self;
  IXAudio2SourceVoice *voice = (IXAudio2SourceVoice *)buffer_ctx;
  assert(voice);
  IXAudio2SourceVoice_Stop(voice, 0, XAUDIO2_COMMIT_NOW);
}

static void
Psv_OnLoopEnd(IXAudio2VoiceCallback *self, void *buffer_ctx)
{
  (void)self; (void)buffer_ctx;
}

static void
Psv_OnVoiceError(IXAudio2VoiceCallback *self, void *buffer_ctx, HRESULT error)
{
  (void)self; (void)buffer_ctx; (void)error;
}

static IXAudio2VoiceCallbackVtbl g_psv_cb_vtbl = {
  .OnVoiceProcessingPassStart = Psv_OnVoiceProcessingPassStart,
  .OnVoiceProcessingPassEnd = Psv_OnVoiceProcessingPassEnd,
  .OnStreamEnd = Psv_OnStreamEnd,
  .OnBufferStart = Psv_OnBufferStart,
  .OnBufferEnd = Psv_OnBufferEnd,
  .OnLoopEnd = Psv_OnLoopEnd,
  .OnVoiceError = Psv_OnVoiceError,
};
static IXAudio2VoiceCallback g_psv_cb = {
  .lpVtbl = &g_psv_cb_vtbl,
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
      &g_optimal_fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO,
      &g_psv_cb, NULL, NULL)))
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
  for (uint32_t i = 0; i < AUD_MAX_SOURCE_VOICES; ++i) {
    if (aud->source_voices[i]) {
      IXAudio2SourceVoice_DestroyVoice(aud->source_voices[i]);
      aud->source_voices[i] = NULL;
    }
  }
  if (aud->mastering_voice) {
    IXAudio2MasteringVoice_DestroyVoice(aud->mastering_voice);
    aud->mastering_voice = NULL;
  }
  if (aud->engine) MFShutdown();
  SAFE_RELEASE(aud->engine);
}

IXAudio2SourceVoice *find_idle_source_voice(AudContext *aud)
{
  assert(aud);
  if (aud->engine == NULL) return NULL;

  for (uint32_t i = 0; i < AUD_MAX_SOURCE_VOICES; ++i) {
    XAUDIO2_VOICE_STATE state;
    IXAudio2SourceVoice_GetState(aud->source_voices[i], &state,
      XAUDIO2_VOICE_NOSAMPLESPLAYED);
    if (state.BuffersQueued == 0) {
      IXAudio2SourceVoice *voice = aud->source_voices[i];
      IXAudio2SourceVoice_SetEffectChain(voice, NULL);
      IXAudio2SourceVoice_SetSourceSampleRate(voice, g_optimal_fmt.
        nSamplesPerSec);
      IXAudio2SourceVoice_SetVolume(voice, 1.0f, XAUDIO2_COMMIT_NOW);
      IXAudio2SourceVoice_SetFrequencyRatio(voice, 1.0f, XAUDIO2_COMMIT_NOW);
      return voice;
    }
  }
  return NULL;
}
