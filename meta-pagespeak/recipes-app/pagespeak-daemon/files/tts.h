/**
 * @file   tts.h
 * @brief  Text-to-speech interface for PageSpeak daemon.
 *
 * Implemented with the espeak C API (espeak/speak_lib.h).
 * Speech rate is configurable at compile time via -DTTS_SPEECH_RATE=<wpm>
 * (default: 150 words per minute).
 *
 * Empty input ("") causes the engine to speak a "no text detected" fallback.
 * NULL input is rejected with a false return value.
 */

#ifndef TTS_H
#define TTS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the espeak TTS engine.
 *
 * Must be called once before tts_speak(). Sets speech rate to TTS_SPEECH_RATE.
 *
 * @return true on success, false on error
 */
bool tts_init(void);

/**
 * @brief Synthesize and speak text through ALSA audio output.
 *
 * Blocks until speech playback completes (calls espeak_Synchronize).
 * Empty string triggers a "no text detected" fallback message.
 *
 * @param text UTF-8 text to speak; must not be NULL
 * @return true on success, false on error or if TTS not initialized
 */
bool tts_speak(const char *text);

/**
 * @brief Terminate the espeak TTS engine and free resources.
 */
void tts_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* TTS_H */
