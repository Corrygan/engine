// The single translation unit that compiles the miniaudio implementation. Kept
// separate so editing AudioEngine.cpp doesn't trigger a (slow) miniaudio rebuild.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING            // playback only; we never write audio files
#include <miniaudio.h>
