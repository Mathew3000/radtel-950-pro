/*
 * The OEM updater's "Upgrade Mode" (BootHelper.cs, HandShake_0) sends
 * "PROGRAMBT9000U" and waits for 0x06, then sends "UPDATE" and waits for
 * 0x06 again. 80 ms later it starts talking to the bootloader (command
 * 0x42). The stock firmware answers both, shows an update screen and resets
 * 30 ms after the second ACK (0x080115f0, 0x0801ca1c, 0x0801bd50); the
 * bootloader then picks up the session.
 *
 * Doing the same here means the updater can put stock firmware back without
 * opening the radio. Its "Flashing Mode" (power on holding side keys 3 and
 * 4) does not involve the application at all and keeps working regardless.
 */
#include <string.h>

#include "update.h"
#include "uart.h"
#include "clock.h"
#include "lcd.h"
#include "log.h"

#define ACK 0x06

static const char hello[] = "PROGRAMBT9000U";
static const char update[] = "UPDATE";

static char window[sizeof(hello) - 1];
static bool program_mode;

void update_init(void)
{
    uart_init();
}

static bool ends_with(const char *s)
{
    size_t n = strlen(s);
    return memcmp(window + sizeof(window) - n, s, n) == 0;
}

static void restart_into_bootloader(void)
{
    /* The updater talks to the bootloader 80 ms after the ACK: keep this short */
    lcd_draw_string(8, 8, "UPDATE MODE", LCD_YELLOW, LCD_BLACK);
    log_printf("update requested, resetting\n");
    clock_delay_ms(30);
    NVIC_SystemReset();
}

void update_poll(void)
{
    uint8_t c;

    while (uart_getc(&c)) {
        memmove(window, window + 1, sizeof(window) - 1);
        window[sizeof(window) - 1] = (char)c;

        if (ends_with(hello)) {
            program_mode = true;
            uart_putc(ACK);
            log_printf("updater handshake\n");
        } else if (program_mode && ends_with(update)) {
            uart_putc(ACK);
            restart_into_bootloader();
        }
    }
}
