// host/api.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define API __declspec(dllexport)
#else
  #define API __attribute__((visibility("default")))
#endif

typedef struct VstHandle VstHandle;

API VstHandle* VstCreate(const char* pluginPath, double sampleRate, int blockSize, int channels);
API void       VstDestroy(VstHandle* h);
API void       VstSetParam(VstHandle* h, uint32_t id, float norm);
API void       VstNoteOn (VstHandle* h, int note, float vel);
API void       VstNoteOff(VstHandle* h, int note);
API void       VstSetLatency(VstHandle* h, uint32_t samples);
API int        VstProcess(VstHandle* h, float* outInterleaved, int frames);
API bool       VstReconfigure(VstHandle* h, double sampleRate, int blockSize, int channels, int processMode);

#ifdef __cplusplus
}
#endif
