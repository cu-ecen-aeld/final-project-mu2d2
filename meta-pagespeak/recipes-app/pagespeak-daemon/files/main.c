/**
 * @file   main.c
 * @brief  PageSpeak daemon - orchestrates button events, camera capture,
 *         preprocessing, OCR, and TTS for the PageSpeak device.
 *
 * Pipeline: button press → capture frame → preprocess → OCR → TTS
 *
 * Pipeline is fully implemented: capture → preprocess → OCR → espeak TTS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "capture.h"
#include "preprocess.h"
#include "ocr.h"
#include "tts.h"

#define BTN_DEVICE_PATH "/dev/pagespeak-btn"
#define CAM_DEVICE_PATH "/dev/pagespeak-cam"

/**
 * @brief Button event structure - must match kernel module definition in pagespeak-btn.c
 */
struct btn_event {
    uint32_t count;
    int64_t  timestamp_ns;
};

/* Signal handling flag - set by SIGINT/SIGTERM handler */
static volatile sig_atomic_t g_stop = 0;

/* Processing state flag - prevents overlapping capture cycles */
static volatile sig_atomic_t g_processing = 0;

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 */
static void sig_handler(int sig)
{
    (void)sig;
    g_stop = 1;
}

/**
 * @brief Run the full capture → preprocess → OCR → TTS pipeline.
 * @return true on success, false on error
 */
static bool run_pipeline(void)
{
    struct capture_frame frame = {0};
    struct preprocess_result preprocessed = {0};
    char *text = NULL;
    bool success = false;
    struct capture_ctx *cam_ctx;

    /* Step 1: Open camera and capture frame */
    cam_ctx = capture_open(CAM_DEVICE_PATH);
    if (!cam_ctx) {
        syslog(LOG_ERR, "pipeline: failed to open camera");
        return false;
    }

    if (!capture_frame(cam_ctx, &frame)) {
        syslog(LOG_ERR, "pipeline: capture failed");
        capture_close(cam_ctx);
        return false;
    }
    capture_close(cam_ctx);
    syslog(LOG_INFO, "pipeline: captured %zu byte frame", frame.size);

    /* Step 2: Preprocess image */
    if (!preprocess_image(&frame, &preprocessed)) {
        syslog(LOG_ERR, "pipeline: preprocessing failed");
        goto cleanup;
    }

    /* Step 3: Run OCR */
    if (!ocr_extract(&preprocessed, &text)) {
        syslog(LOG_ERR, "pipeline: OCR failed");
        goto cleanup;
    }

    /* Step 4: Speak text (empty → "no text detected" handled by tts_speak) */
    syslog(LOG_INFO, "pipeline: extracted %zu chars, speaking",
           text ? strlen(text) : (size_t)0);
    if (!tts_speak(text ? text : "")) {
        syslog(LOG_ERR, "pipeline: TTS failed");
        goto cleanup;
    }

    success = true;

cleanup:
    ocr_free_text(text);
    preprocess_free(&preprocessed);
    capture_free(&frame);
    return success;
}

int main(int argc, char *argv[])
{
    int btn_fd = -1;
    struct btn_event event;
    ssize_t n;

    (void)argc;
    (void)argv;

    /* Install signal handlers */
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* Open syslog */
    openlog("pagespeak-daemon", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "starting");

    /* Initialize subsystems */
    if (!ocr_init()) {
        syslog(LOG_ERR, "failed to initialize OCR");
        closelog();
        return 1;
    }

    if (!tts_init()) {
        syslog(LOG_ERR, "failed to initialize TTS");
        ocr_cleanup();
        closelog();
        return 1;
    }

    /* Open button device */
    syslog(LOG_INFO, "opening %s", BTN_DEVICE_PATH);
    btn_fd = open(BTN_DEVICE_PATH, O_RDONLY);
    if (btn_fd < 0) {
        syslog(LOG_ERR, "failed to open %s: %s", BTN_DEVICE_PATH, strerror(errno));
        tts_cleanup();
        ocr_cleanup();
        closelog();
        return 1;
    }

    syslog(LOG_INFO, "ready, waiting for button presses");

    /* Main event loop - blocks on button device read */
    while (!g_stop) {
        n = read(btn_fd, &event, sizeof(event));

        if (n < 0) {
            if (errno == EINTR) {
                continue;  /* Signal interrupted, check g_stop */
            }
            syslog(LOG_ERR, "button read error: %s", strerror(errno));
            break;
        }

        if ((size_t)n < sizeof(event)) {
            syslog(LOG_WARNING, "short read: got %zd bytes, expected %zu", n, sizeof(event));
            continue;
        }

        /* Ignore button press if already processing */
        if (g_processing) {
            syslog(LOG_DEBUG, "button press #%u ignored (busy)", event.count);
            continue;
        }

        syslog(LOG_INFO, "button press #%u, starting pipeline", event.count);
        g_processing = 1;

        run_pipeline();

        g_processing = 0;
        syslog(LOG_INFO, "button press #%u complete", event.count);
    }

    /* Cleanup */
    close(btn_fd);
    tts_cleanup();
    ocr_cleanup();
    syslog(LOG_INFO, "exiting");
    closelog();
    return 0;
}
