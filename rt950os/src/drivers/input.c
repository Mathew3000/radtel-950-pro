/*
 * Keypad, encoder and PTT inputs.
 *
 * Follows the OEM Keypad_ScanMatrix (0x08016218) and its tables at
 * 0x0803013a (column set masks), 0x08030144 (column clear masks) and
 * 0x0803014e (key codes), read with the application base 0x08002C00:
 * - PE5 low gives code 0 and PA12 low gives code 1 (direct keys).
 * - Otherwise PC0-PC3 are all driven low; if PD4-PD7 all read high, no
 *   matrix key is down.
 * - Five steps then drive: no column low, PC0 low, PC1 low, PC2 low, PC3 low.
 *   In each step exactly one of PD7, PD6, PD5, PD4 reading low picks the code
 *   at key_codes[step * 4 + 0..3].
 * Which label each code belongs to is still to be confirmed on the radio.
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

static const uint16_t col_set[5] = { 0x0F, 0x0E, 0x0D, 0x0B, 0x07 };
static const uint16_t col_clr[5] = { 0x00, 0x01, 0x02, 0x04, 0x08 };
static const uint8_t key_codes[5][4] = {
    { 0x0A, 0x0E, 0x12, 0x0D },
    { 0x06, 0x03, 0x09, 0x02 },
    { 0x0C, 0x10, 0x14, 0x15 },
    { 0x0B, 0x0F, 0x13, 0x11 },
    { 0x07, 0x08, 0x04, 0x05 },
};

static uint8_t scan_matrix(void)
{
    uint8_t key = INPUT_KEY_NONE;

    COL_PORT->clr = COL_PINS;
    settle();
    if ((ROW_PORT->idt & ROW_PINS) == ROW_PINS) {
        COL_PORT->scr = COL_PINS;
        return INPUT_KEY_NONE;
    }

    for (unsigned step = 0; step < 5 && key == INPUT_KEY_NONE; step++) {
        COL_PORT->scr = col_set[step];
        COL_PORT->clr = col_clr[step];
        settle();

        switch (ROW_PORT->idt & ROW_PINS) {
        case 0x70: key = key_codes[step][0]; break; /* PD7 low */
        case 0xB0: key = key_codes[step][1]; break; /* PD6 low */
        case 0xD0: key = key_codes[step][2]; break; /* PD5 low */
        case 0xE0: key = key_codes[step][3]; break; /* PD4 low */
        default: break;
        }
    }

    COL_PORT->scr = COL_PINS;
    return key;
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
