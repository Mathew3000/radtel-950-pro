/*
 * Debug log over a SEGGER RTT compatible up-buffer.
 *
 * RTT needs no pins: the debugger finds the control block in RAM by its ID
 * string and reads the ring buffer while the target runs. J-Link (RTT Viewer),
 * OpenOCD ("rtt setup" / "rtt start") and pyOCD all support it. The layout
 * below follows the published RTT control block format with one up-buffer and
 * no down-buffers. When no debugger reads the buffer, writes are dropped once
 * it is full instead of blocking.
 */
#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "at32f403a_407.h"

#define LOG_BUF_SIZE 1024

typedef struct {
    const char *name;
    char *buf;
    uint32_t size;
    volatile uint32_t wr;
    volatile uint32_t rd;
    uint32_t flags;
} rtt_ring_t;

typedef struct {
    char id[16];
    int32_t max_up;
    int32_t max_down;
    rtt_ring_t up[1];
} rtt_cb_t;

static char log_buf[LOG_BUF_SIZE];
static rtt_cb_t rtt_cb;

void log_init(void)
{
    rtt_cb.max_up = 1;
    rtt_cb.max_down = 0;
    rtt_cb.up[0].name = "Terminal";
    rtt_cb.up[0].buf = log_buf;
    rtt_cb.up[0].size = sizeof(log_buf);
    rtt_cb.up[0].wr = 0;
    rtt_cb.up[0].rd = 0;
    rtt_cb.up[0].flags = 0; /* drop data when full */

    /* Write the ID last, so a debugger scanning RAM never sees a half-set-up block. */
    __DMB();
    memcpy(rtt_cb.id, "SEGGER RTT\0\0\0\0\0\0", sizeof(rtt_cb.id));
    __DMB();
}

void log_write(const char *s, size_t len)
{
    rtt_ring_t *r = &rtt_cb.up[0];
    uint32_t wr = r->wr;

    while (len--) {
        uint32_t next = (wr + 1) % r->size;
        if (next == r->rd) {
            break;
        }
        r->buf[wr] = *s++;
        wr = next;
    }
    __DMB();
    r->wr = wr;
}

void log_printf(const char *fmt, ...)
{
    char line[128];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    if (n > 0) {
        log_write(line, (size_t)n < sizeof(line) ? (size_t)n : sizeof(line) - 1);
    }
}
