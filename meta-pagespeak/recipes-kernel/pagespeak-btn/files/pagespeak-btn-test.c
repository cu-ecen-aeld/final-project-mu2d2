/**
 * @file   pagespeak-btn-test.c
 * @brief  Userspace validation program for the pagespeak-btn kernel module.
 *         Opens /dev/pagespeak-btn and reads btn_event structs in a blocking
 *         loop. Each event is timestamped using the realtime clock, logged to
 *         syslog, and printed to stdout so the serial console also shows output.
 *
 *         DoD validation: run this program, press the button 20 times, then
 *         verify the syslog contains exactly 20 pagespeak-btn-test entries:
 *           grep "pagespeak-btn-test" /var/log/messages | wc -l
 *
 *         Usage: pagespeak-btn-test
 *         Exit:  Ctrl+C
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

// Path to the character device created by pagespeak-btn.ko
#define DEVICE_PATH "/dev/pagespeak-btn"

// Buffer length for "YYYY-MM-DDTHH:MM:SS.mmmZ" plus null terminator
#define TIMESTAMP_BUF_LEN 32

/**
 * @brief Event structure read from /dev/pagespeak-btn.
 *        Must match the kernel module definition exactly; both sides use
 *        fixed-width integer types to avoid ABI mismatch between kernel and
 *        userspace.
 */
struct btn_event {
    uint32_t count;        // press counter maintained by the kernel module
    int64_t  timestamp_ns; // ktime_get() value at press acceptance, nanoseconds
};

// Set to 1 by the SIGINT/SIGTERM handler to break the read loop cleanly
static volatile sig_atomic_t stop = 0;

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 *        Sets the stop flag so the main loop exits gracefully after the
 *        current read() call returns.
 *
 * @param sig Signal number received (not inspected, both signals handled the same way)
 *
 * @return void
 */
static void sig_handler(int sig)
{
    (void)sig; // suppress unused parameter warning
    stop = 1;
}

/**
 * @brief Formats the current wall-clock time as an ISO 8601 UTC string.
 *        The kernel's timestamp_ns is a monotonic ktime value and is not
 *        used for wall time. CLOCK_REALTIME is sampled at the moment this
 *        function is called and provides the human-readable timestamp.
 *
 * @param buf Output character buffer to write the formatted string into
 * @param len Length of buf in bytes, must be at least TIMESTAMP_BUF_LEN
 *
 * @return Pointer to buf, suitable for direct use in printf or syslog
 */
static char *format_wallclock(char *buf, size_t len)
{
    struct timespec ts;
    struct tm       tm_info;

    // Sample the realtime clock for a human-readable wall-clock timestamp
    clock_gettime(CLOCK_REALTIME, &ts);
    gmtime_r(&ts.tv_sec, &tm_info);

    snprintf(buf, len, "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
             tm_info.tm_year + 1900,
             tm_info.tm_mon  + 1,
             tm_info.tm_mday,
             tm_info.tm_hour,
             tm_info.tm_min,
             tm_info.tm_sec,
             ts.tv_nsec / 1000000L); // convert ns remainder to ms

    return buf;
}

/**
 * @brief Main entry point. Installs signal handlers, opens the character
 *        device, and reads button events in a blocking loop until Ctrl+C
 *        or a read error occurs. Each event is logged to syslog and stdout.
 *
 * @param argc Argument count (not used)
 * @param argv Argument vector (not used)
 *
 * @return 0 on clean exit, 1 on fatal error opening the device
 */
int main(int argc, char *argv[])
{
    int              fd;
    struct btn_event event;
    ssize_t          n;
    char             timebuf[TIMESTAMP_BUF_LEN];

    (void)argc;
    (void)argv;

    // Install handlers so Ctrl+C and SIGTERM allow a clean exit
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    // Open syslog connection with PID in each message, facility LOG_USER
    openlog("pagespeak-btn-test", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "starting, opening %s", DEVICE_PATH);

    printf("pagespeak-btn-test: opening %s\n", DEVICE_PATH);
    fflush(stdout);

    // Open the character device in read-only mode
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0)
    {
        syslog(LOG_ERR, "failed to open %s: %s", DEVICE_PATH, strerror(errno));
        fprintf(stderr, "pagespeak-btn-test: failed to open %s: %s\n",
                DEVICE_PATH, strerror(errno));
        closelog();
        return 1;
    }

    printf("pagespeak-btn-test: ready, waiting for button presses (Ctrl+C to stop)\n");
    fflush(stdout);

    // Blocking read loop, each read() call sleeps in the kernel wait queue
    // until the IRQ handler calls wake_up_interruptible
    while (!stop)
    {
        n = read(fd, &event, sizeof(event));

        if (n < 0)
        {
            // EINTR means a signal woke the syscall, re-check the stop flag
            if (errno == EINTR)
                continue;
            syslog(LOG_ERR, "read error: %s", strerror(errno));
            fprintf(stderr, "pagespeak-btn-test: read error: %s\n", strerror(errno));
            break;
        }

        // Guard against a short read returning less than a full event struct
        if ((size_t)n < sizeof(event))
        {
            syslog(LOG_WARNING, "short read: got %zd bytes, expected %zu",
                   n, sizeof(event));
            continue;
        }

        // Capture wall-clock time for the human-readable log entry
        format_wallclock(timebuf, sizeof(timebuf));

        // Log to syslog, visible in /var/log/messages or journalctl
        syslog(LOG_INFO, "button press #%u at %s (kernel ts %lld ns)",
               event.count, timebuf, (long long)event.timestamp_ns);

        // Print to stdout so the serial console also shows each event live
        printf("pagespeak-btn-test: button press #%u at %s\n",
               event.count, timebuf);
        fflush(stdout);
    }

    // Summary line after loop exits
    printf("pagespeak-btn-test: exiting after %u total presses\n", event.count);
    syslog(LOG_INFO, "exiting after %u total presses", event.count);

    close(fd);
    closelog();
    return 0;
}
