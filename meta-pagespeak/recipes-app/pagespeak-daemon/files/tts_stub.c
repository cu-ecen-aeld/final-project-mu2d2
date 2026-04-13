/**
 * @file   tts_stub.c
 * @brief  STUB TTS implementation for PageSpeak daemon.
 *
 * This stub logs text to syslog without actual speech synthesis.
 * Replace with espeak-ng implementation in future issue.
 *
 * TODO: Implement with espeak-ng:
 * - espeak_Initialize() with AUDIO_OUTPUT_PLAYBACK
 * - espeak_Synth() to synthesize and play speech
 * - espeak_Synchronize() to wait for completion
 */

#include "tts.h"
#include <syslog.h>
#include <string.h>

bool tts_init(void)
{
    syslog(LOG_INFO, "tts_init: STUB - espeak-ng not implemented");
    return true;
}

bool tts_speak(const char *text)
{
    if (!text) {
        return false;
    }

    /* STUB: Log text that would be spoken */
    size_t len = strlen(text);

    /* Log first 100 chars to avoid flooding syslog */
    if (len > 100) {
        syslog(LOG_INFO, "tts_speak: STUB - would speak: \"%.100s...\" (%zu chars total)",
               text, len);
    } else {
        syslog(LOG_INFO, "tts_speak: STUB - would speak: \"%s\"", text);
    }

    return true;
}

void tts_cleanup(void)
{
    syslog(LOG_INFO, "tts_cleanup: STUB - nothing to clean up");
}
