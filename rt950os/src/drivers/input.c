/*
 * Keypad, encoder and PTT inputs.
 *
 * Pins come from docs/pinout.md and the OEM Keypad_ScanMatrix (0x08013618):
 * - PE5 and PA12 are read first as direct keys (low = pressed).
 * - Otherwise PC0-PC3 are all driven low; if PD4-PD7 all read high, no
 *   matrix key is down. Then five steps drive different column patterns from
 *   tables at 0x0803013a/0x08030144 and read PD4-PD7.
 *
 * The literal pool points those tables (and the key map at 0x0803014e) into
 * what decrypted.bin holds as a GBK pinyin table, so their contents are not
 * known yet. Until they are, this driver scans one column low at a time plus a
 * fifth step with no column driven, and reports raw positions for mapping on
 * hardware.
 */
#include "input.h"
#include "board.h"

#define COL_PORT   GPIOC
#define COL_PINS   (GPIO_PINS_0 | GPIO_PINS_1 | GPIO_PINS_2 | GPIO_PINS_3)
#define ROW_PORT   GPIOD
#define ROW_PINS   (GPIO_PINS_4 | GPIO_PINS_5 | GPIO_PINS_6 | GPIO_PINS_7)
#define KEY0_PORT  GPIOE
#define KEY0_PIN   GPIO_PINS_5
#define KEY1_PORT  GPIOA
#define KEY1_PIN   GPIO_PINS_12

static void settle(void)
{
    /* About 10 us at 240 MHz, like the OEM's FUN_0800b7c2(10) between steps */
    for (volatile unsigned i = 0; i < 600; i++) {
    }
}

static void pin_input(gpio_type *port, uint16_t pins, gpio_pull_type pull)
{
    gpio_init_type cfg;

    gpio_default_para_init(&cfg);
    cfg.gpio_pins = pins;
    cfg.gpio_mode = GPIO_MODE_INPUT;
    cfg.gpio_pull = pull;
    gpio_init(port, &cfg);
}

void input_init(void)
{
    gpio_init_type cfg;

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOD_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);

    COL_PORT->scr = COL_PINS;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = COL_PINS;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(COL_PORT, &cfg);

    pin_input(ROW_PORT, ROW_PINS, GPIO_PULL_UP);
    pin_input(KEY0_PORT, KEY0_PIN, GPIO_PULL_UP);
    /* PA12 is also USB D+, so no pull-up that could upset the USB bus */
    pin_input(KEY1_PORT, KEY1_PIN, GPIO_PULL_NONE);
    pin_input(BOARD_PTT_PORT, BOARD_PTT_PIN | BOARD_PTT_EXT_PIN, GPIO_PULL_UP);
    pin_input(BOARD_ENC_PORT, BOARD_ENC_A_PIN | BOARD_ENC_B_PIN, GPIO_PULL_UP);
}

static uint8_t rows_low(void)
{
    /* PD4..PD7 -> bits 0..3, set when the row reads low */
    return (uint8_t)(~(ROW_PORT->idt >> 4) & 0x0F);
}

static uint8_t scan_matrix(void)
{
    COL_PORT->clr = COL_PINS;
    settle();
    if (rows_low() == 0) {
        COL_PORT->scr = COL_PINS;
        return INPUT_KEY_NONE;
    }

    for (unsigned step = 0; step < 5; step++) {
        COL_PORT->scr = COL_PINS;
        if (step < 4) {
            COL_PORT->clr = (uint16_t)(GPIO_PINS_0 << step);
        }
        settle();

        uint8_t rows = rows_low();
        for (unsigned row = 0; row < 4; row++) {
            if (rows & (1U << row)) {
                COL_PORT->scr = COL_PINS;
                return (uint8_t)(2 + step * 4 + row);
            }
        }
    }

    COL_PORT->scr = COL_PINS;
    return INPUT_KEY_NONE;
}

void input_read(input_raw_t *out)
{
    if (!(KEY0_PORT->idt & KEY0_PIN)) {
        out->key = 0;
    } else if (!(KEY1_PORT->idt & KEY1_PIN)) {
        out->key = 1;
    } else {
        out->key = scan_matrix();
    }

    uint32_t ptt = BOARD_PTT_PORT->idt;
    out->ptt = !(ptt & BOARD_PTT_PIN);
    out->ptt_ext = !(ptt & BOARD_PTT_EXT_PIN);

    uint32_t enc = BOARD_ENC_PORT->idt;
    out->enc_ab = (uint8_t)(((enc & BOARD_ENC_A_PIN) ? 2 : 0) | ((enc & BOARD_ENC_B_PIN) ? 1 : 0));
}

int32_t input_encoder_poll(void)
{
    /* Quadrature table indexed by (previous AB << 2) | current AB */
    static const int8_t table[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
    static uint8_t prev;
    static int32_t sub;
    int32_t steps;

    uint32_t enc = BOARD_ENC_PORT->idt;
    uint8_t ab = (uint8_t)(((enc & BOARD_ENC_A_PIN) ? 2 : 0) | ((enc & BOARD_ENC_B_PIN) ? 1 : 0));

    sub += table[(prev << 2) | ab];
    prev = ab;

    /* Four transitions per detent on a typical encoder; to be confirmed */
    steps = sub / 4;
    sub -= steps * 4;
    return steps;
}
