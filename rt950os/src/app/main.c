/*
 * Bring-up test image.
 *
 * Sets up the clocks, draws a test screen on the TFT and blinks the LCD
 * backlight at 1 Hz. A blink is visible whatever the backlight polarity turns
 * out to be. The colour bars show whether the colour order and orientation
 * are right, and the seconds counter proves the clock runs at the expected
 * speed. Below them, raw key codes, PTT levels and an encoder counter let us
 * map the keypad on the radio.
 *
 * Both BK4829s get the OEM register setup (receive path only, no TX), and a
 * few registers are read back and shown. Matching values prove the 3-wire bus
 * works in both directions.
 *
 * Then the radio receives FM on RX_DEFAULT_FREQ. The encoder tunes in
 * 12.5 kHz steps, the screen shows the RSSI, and the audio opens while the
 * RSSI is above SQ_OPEN_DBM. Nothing here can transmit.
 *
 * The OEM updater's "Upgrade Mode" works from this image too (update.c), and
 * turning the power switch off switches the radio off (power.c).
 */
#include <string.h>

#include "clock.h"
#include "backlight.h"
#include "lcd.h"
#include "input.h"
#include "log.h"
#include "bk4829.h"
#include "rf.h"
#include "update.h"
#include "power.h"
#include "spi_flash.h"

/*
 * The OEM bootloader starts the application and may leave interrupts enabled.
 * Point VTOR at our vector table and start from a clean NVIC, so a leftover
 * interrupt cannot land in the bootloader's handlers or Default_Handler.
 */
static void nvic_clear(void)
{
    extern const uint32_t g_pfnVectors[];

    __disable_irq();
    /* SystemInit points VTOR at 0x08000000, which is the bootloader's table */
    SCB->VTOR = (uint32_t)g_pfnVectors;
    __DSB();
    SysTick->CTRL = 0;
    for (unsigned i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    __enable_irq();
}

static void draw_test_screen(void)
{
    static const uint16_t bars[] = {
        LCD_WHITE, LCD_YELLOW, LCD_CYAN, LCD_GREEN, LCD_RED, LCD_BLUE, LCD_BLACK, LCD_GREY,
    };
    const uint16_t bar_w = LCD_WIDTH / (sizeof(bars) / sizeof(bars[0]));

    lcd_fill(LCD_BLACK);
    lcd_draw_string(8, 8, "rt950os", LCD_WHITE, LCD_BLACK);

    for (unsigned i = 0; i < sizeof(bars) / sizeof(bars[0]); i++) {
        lcd_fill_rect(i * bar_w, 56, bar_w, 40, bars[i]);
    }

    /* Corner markers: red top left, green bottom right */
    lcd_fill_rect(0, 0, 4, 4, LCD_RED);
    lcd_fill_rect(LCD_WIDTH - 4, LCD_HEIGHT - 4, 4, 4, LCD_GREEN);
}

/* Format a signed decimal into buf (at least 12 bytes), right-aligned to width. */
static const char *fmt_int(char *buf, int32_t v, unsigned width)
{
    char tmp[12];
    unsigned n = 0, pos = 0;
    uint32_t u = v < 0 ? (uint32_t)-v : (uint32_t)v;

    do {
        tmp[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u && n < 10);
    if (v < 0) {
        tmp[n++] = '-';
    }
    while (width > n && pos < 11) {
        buf[pos++] = ' ';
        width--;
    }
    while (n) {
        buf[pos++] = tmp[--n];
    }
    buf[pos] = '\0';
    return buf;
}

static void draw_field(uint16_t y, const char *label, const char *value)
{
    uint16_t x = lcd_draw_string(8, y, label, LCD_GREY, LCD_BLACK);
    lcd_draw_string(x, y, value, LCD_WHITE, LCD_BLACK);
}

static void draw_inputs(const input_raw_t *in, int32_t enc_count)
{
    char buf[12];

    draw_field(136, "key ", in->key == INPUT_KEY_NONE ? "  --" : fmt_int(buf, in->key, 4));
    draw_field(156, "ptt ", in->ptt ? "DOWN" : "  up");
    buf[0] = (in->aux & 2) ? '1' : '0';
    buf[1] = (in->aux & 1) ? '1' : '0';
    buf[2] = '\0';
    draw_field(176, "PE0 PC15 ", buf);
    draw_field(196, "enc ", fmt_int(buf, enc_count, 6));
    buf[0] = (in->enc_ab & 2) ? '1' : '0';
    buf[1] = (in->enc_ab & 1) ? '1' : '0';
    buf[2] = '\0';
    draw_field(216, "A B ", buf);
}

static const uint8_t bk_check_regs[] = { 0x37, 0x7B, 0x1D, 0x3E };
static const uint16_t bk_check_vals[] = { 0x9D1F, 0x73DC, 0xE555, 0x94C6 };

static void hex16(char *out, uint16_t v)
{
    static const char digits[] = "0123456789abcdef";
    for (int i = 3; i >= 0; i--) {
        out[i] = digits[v & 0xF];
        v >>= 4;
    }
}

/* Read back the check registers of one chip, log them, and draw one status line. */
static void bk_check(bk_chip_t chip, uint16_t y)
{
    char line[32] = "bk0 ok 0/4 37=";
    unsigned ok = 0;
    uint16_t first = 0;

    line[2] = (char)('0' + chip);
    for (unsigned i = 0; i < sizeof(bk_check_regs); i++) {
        uint16_t v = bk4829_read(chip, bk_check_regs[i]);
        log_printf("bk%u reg %02x = %04x (wrote %04x)\n", chip, bk_check_regs[i], v, bk_check_vals[i]);
        if (v == bk_check_vals[i]) {
            ok++;
        }
        if (i == 0) {
            first = v;
        }
    }
    line[7] = (char)('0' + ok);
    hex16(&line[14], first);
    line[18] = '\0';
    if (ok == 0) {
        line[4] = '?';
        line[5] = '?';
    }
    lcd_draw_string(8, y, line, ok == sizeof(bk_check_regs) ? LCD_GREEN : LCD_RED, LCD_BLACK);
}

/* 10 Hz units: 145.500 MHz, 12.5 kHz steps */
#define RX_DEFAULT_FREQ 14550000U
#define RX_STEP         1250U
#define RX_MIN_FREQ     1800000U   /* 18 MHz */
#define RX_MAX_FREQ     52000000U  /* 520 MHz */
#define SQ_OPEN_DBM     (-110)
#define SQ_CLOSE_DBM    (-115)

static void draw_freq(uint32_t freq)
{
    char line[20];
    uint32_t mhz = freq / 100000U;
    uint32_t frac = (freq % 100000U) / 10U; /* 100 Hz digits */

    line[0] = 'r';
    line[1] = 'x';
    line[2] = ' ';
    line[3] = (char)(mhz >= 100 ? '0' + mhz / 100 : ' ');
    line[4] = (char)(mhz >= 10 ? '0' + (mhz / 10) % 10 : ' ');
    line[5] = (char)('0' + mhz % 10);
    line[6] = '.';
    for (int i = 3; i >= 0; i--) {
        line[7 + i] = (char)('0' + frac % 10);
        frac /= 10;
    }
    line[11] = ' ';
    line[12] = 'b';
    line[13] = 'k';
    line[14] = (char)('0' + rf_chip(freq));
    line[15] = '\0';
    lcd_draw_string(8, 284, line, LCD_WHITE, LCD_BLACK);
}

static void draw_rssi(int16_t dbm, bool open)
{
    char buf[12];
    uint16_t x = lcd_draw_string(8, 302, "rssi ", LCD_GREY, LCD_BLACK);

    x = lcd_draw_string(x, 302, fmt_int(buf, dbm, 4), LCD_WHITE, LCD_BLACK);
    lcd_draw_string(x, 302, open ? " dBm OPEN" : " dBm    ", open ? LCD_GREEN : LCD_WHITE, LCD_BLACK);
}

/*
 * Read the SPI flash ID and the stock calibration block, log the block over
 * RTT as a hex dump and show the ID and a checksum on screen. Read only.
 */
static uint8_t cal[SPI_FLASH_CAL_SIZE];

static void show_flash(void)
{
    char line[32] = "flash ";
    uint32_t id, sum = 0;

    spi_flash_init();
    id = spi_flash_read_id();
    spi_flash_read(SPI_FLASH_CAL_ADDR, cal, sizeof(cal));

    log_printf("spi flash id %06lx, calibration at %04x:\n", (unsigned long)id, SPI_FLASH_CAL_ADDR);
    for (unsigned off = 0; off < sizeof(cal); off += 16) {
        char hex[16 * 3 + 1];
        for (unsigned i = 0; i < 16; i++) {
            static const char digits[] = "0123456789abcdef";
            hex[i * 3] = digits[cal[off + i] >> 4];
            hex[i * 3 + 1] = digits[cal[off + i] & 0xF];
            hex[i * 3 + 2] = ' ';
            sum += cal[off + i];
        }
        hex[16 * 3 - 1] = '\0';
        log_printf("%04x: %s\n", SPI_FLASH_CAL_ADDR + off, hex);
    }

    hex16(&line[6], (uint16_t)(id >> 8));
    hex16(&line[10], (uint16_t)(id << 8));
    line[12] = ' ';
    memcpy(&line[13], "cal ", 4);
    hex16(&line[17], (uint16_t)sum);
    line[21] = '\0';
    lcd_draw_string(8, 28, line, LCD_GREY, LCD_BLACK);
}

int main(void)
{
    uint32_t next = 0;
    uint32_t next_input = 0;
    uint32_t seconds = 0;
    int32_t enc_count = 0;
    uint32_t freq = RX_DEFAULT_FREQ;
    bool sq_open = false;
    bool retune = true;
    uint8_t last_key = INPUT_KEY_NONE;
    char buf[12];

    nvic_clear();
    clock_init();
    log_init();
    log_printf("rt950os bring-up, sysclk %lu Hz\n", (unsigned long)system_core_clock);
    backlight_init();
    backlight_set(true);
    lcd_init();
    input_init();
    draw_test_screen();

    bk4829_bus_init();
    bk4829_init(BK_CHIP_0);
    bk4829_init(BK_CHIP_1);
    bk_check(BK_CHIP_0, 248);
    bk_check(BK_CHIP_1, 268);

    rf_init();
    update_init();
    show_flash();

    for (;;) {
        int32_t steps = input_encoder_poll();

        update_poll();
        power_poll();

        if (steps) {
            int64_t f = (int64_t)freq + (int64_t)steps * RX_STEP;
            enc_count += steps;
            if (f < RX_MIN_FREQ) {
                f = RX_MIN_FREQ;
            } else if (f > RX_MAX_FREQ) {
                f = RX_MAX_FREQ;
            }
            freq = (uint32_t)f;
            retune = true;
        }

        if ((int32_t)(clock_millis() - next_input) >= 0) {
            input_raw_t in;
            next_input += 20;
            input_read(&in);
            draw_inputs(&in, enc_count);

            if (retune) {
                retune = false;
                sq_open = false;
                rf_rx_start(freq);
                draw_freq(freq);
                log_printf("rx %lu0 Hz on bk%u\n", (unsigned long)freq, rf_chip(freq));
            }

            int16_t dbm = bk4829_rssi_dbm(rf_chip(freq));
            bool open = sq_open ? dbm > SQ_CLOSE_DBM : dbm >= SQ_OPEN_DBM;
            if (open != sq_open) {
                sq_open = open;
                rf_audio(open);
                log_printf("squelch %s at %d dBm\n", open ? "open" : "closed", dbm);
            }
            draw_rssi(dbm, sq_open);
            if (in.key != last_key) {
                if (in.key != INPUT_KEY_NONE) {
                    log_printf("key %u\n", in.key);
                }
                last_key = in.key;
            }
        }

        if ((int32_t)(clock_millis() - next) >= 0) {
            next += 500;
            backlight_toggle();
            if ((next / 500) % 2 == 0) {
                draw_field(112, "up ", fmt_int(buf, (int32_t)seconds++, 6));
            }
        }
    }
}
