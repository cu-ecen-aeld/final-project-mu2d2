/**
 * @file   tts.h
 * @brief  Text-to-speech interface for PageSpeak daemon.
 *
 * STUB: This interface will be implemented with espeak-ng in a future issue.
 * The stub implementation logs the text to syslog.
 */

#ifndef TTS_H
#define TTS_H

#include <stdbool.h>

/**
 * @brief Initialize TTS engine.
 *
 * Future implementation will initialize espeak-ng.
 *
 * @return true on success, false on error
 */
bool tts_init(void);

/**
 * @brief Speak text using TTS engine.
 *
 * Future implementation will synthesize speech via espeak-ng
 * and play through ALSA audio output.
 *
 * @param text Text to speak (UTF-8)
 * @return true on success, false on error
 */
bool tts_speak(const char *text);

/**
 * @brief Cleanup TTS engine and free resources.
 */
void tts_cleanup(void);

#endif /* TTS_H */
