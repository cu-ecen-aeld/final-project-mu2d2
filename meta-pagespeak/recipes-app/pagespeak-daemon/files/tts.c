/**
 * @file   tts.c
 * @brief  Text-to-speech implementation for PageSpeak daemon using espeak.
 *
 * Uses the espeak C API to synthesize and play speech through ALSA.
 * Speech rate defaults to TTS_SPEECH_RATE (150 WPM); override at compile time
 * with -DTTS_SPEECH_RATE=<wpm>.
 *
 * Empty or NULL-text behaviour:
 *   - NULL text  → returns false (invalid input)
 *   - Empty text → speaks the TTS_NO_TEXT_MSG fallback string
 */

#include "tts.h"

#include <espeak/speak_lib.h>
#include <syslog.h>
#include <string.h>

#ifndef TTS_SPEECH_RATE
#define TTS_SPEECH_RATE 150
#endif

#define TTS_NO_TEXT_MSG "no text detected"

static bool g_initialized = false;

bool tts_init(void)
{
    int sample_rate = espeak_Initialize(AUDIO_OUTPUT_PLAYBACK, 0, NULL, 0);
    if (sample_rate < 0) {
        syslog(LOG_ERR, "tts_init: espeak_Initialize failed (%d)", sample_rate);
        return false;
    }

    espeak_SetParameter(espeakRATE, TTS_SPEECH_RATE, 0);
    g_initialized = true;
    syslog(LOG_INFO, "tts_init: espeak ready at %d WPM", TTS_SPEECH_RATE);
    return true;
}

bool tts_speak(const char *text)
{
    if (!text) {
        syslog(LOG_WARNING, "tts_speak: NULL text");
        return false;
    }

    if (!g_initialized) {
        syslog(LOG_ERR, "tts_speak: TTS not initialized");
        return false;
    }

    const char *speak_text = (text[0] != '\0') ? text : TTS_NO_TEXT_MSG;
    size_t len = strlen(speak_text);

    espeak_ERROR err = espeak_Synth(speak_text, len + 1, 0, POS_CHARACTER, 0,
                                    espeakCHARS_UTF8, NULL, NULL);
    if (err != EE_OK) {
        syslog(LOG_ERR, "tts_speak: espeak_Synth failed (%d)", (int)err);
        return false;
    }

    err = espeak_Synchronize();
    if (err != EE_OK) {
        syslog(LOG_ERR, "tts_speak: espeak_Synchronize failed (%d)", (int)err);
        return false;
    }

    return true;
}

void tts_cleanup(void)
{
    if (g_initialized) {
        espeak_Terminate();
        g_initialized = false;
        syslog(LOG_INFO, "tts_cleanup: espeak terminated");
    }
}
