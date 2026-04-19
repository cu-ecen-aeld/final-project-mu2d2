/**
 * @file   tts_test.c
 * @brief  Unit tests for the espeak TTS implementation.
 *
 * Tests cover: init/cleanup lifecycle, NULL rejection, empty-string fallback,
 * valid text synthesis, and use-before-init guard.
 *
 * Requires a functional ALSA audio device on the target.
 * Run: ./tts_test
 */

#include "tts.h"

#include <stdio.h>
#include <stdbool.h>

#define PASS "\033[32mPASS\033[0m"
#define FAIL "\033[31mFAIL\033[0m"

static int s_run    = 0;
static int s_passed = 0;

#define CHECK(label, expr) do {             \
    s_run++;                                \
    if (expr) {                             \
        printf("[" PASS "] %s\n", label);   \
        s_passed++;                         \
    } else {                               \
        printf("[" FAIL "] %s\n", label);   \
    }                                       \
} while (0)

int main(void)
{
    printf("=== TTS Unit Tests ===\n\n");

    /* Test 1: init succeeds */
    CHECK("tts_init succeeds", tts_init() == true);

    /* Test 2: speak without init fails (use after cleanup) */
    tts_cleanup();
    CHECK("tts_speak without init returns false", tts_speak("hello") == false);

    /* Test 3: re-init after cleanup succeeds */
    CHECK("tts_init after cleanup succeeds", tts_init() == true);

    /* Test 4: NULL text rejected */
    CHECK("tts_speak(NULL) returns false", tts_speak(NULL) == false);

    /* Test 5: empty string speaks "no text detected" fallback */
    CHECK("tts_speak(\"\") returns true (speaks fallback)", tts_speak("") == true);

    /* Test 6: valid short text speaks successfully */
    CHECK("tts_speak(\"page speak test\") returns true", tts_speak("page speak test") == true);

    tts_cleanup();

    printf("\n=== Results: %d/%d passed ===\n", s_passed, s_run);
    return (s_passed == s_run) ? 0 : 1;
}
