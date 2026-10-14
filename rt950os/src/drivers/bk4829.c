/*
 * BK4829 register access over the 3-wire bus on GPIOE.
 *
 * From the OEM access routines, read with the application base 0x08002C00:
 * write chip 0 at 0x0801f358, write chip 1 at 0x0801f3ac, read chip 0 at
 * 0x0801f28c, read chip 1 at 0x0801ec4c, byte writer at 0x0801e614. The
 * selector at 0x0800e3d0 picks a pair of them.
 * - SCK PE10, bidirectional SDA PE11, chip select PE8 (chip 0) / PE15 (chip 1),
 *   all active low and bit-banged.
 * - Bits go out MSB first: SDA is set while SCK is low, the chip latches on
 *   the SCK rising edge. A write is the register byte then 16 data bits.
 * - A read sends the register byte with bit 7 set, turns SDA into an input
 *   (pulled up) and samples 16 bits just after each SCK rising edge.
 * - The OEM delay between edges is about 2 us; we use 2 us.
 *
 * docs/pinout.md calls PE8/PE10/PE11/PE15 a band relay shift register; the
 * access routines show it is this bus. This is the same 3-wire protocol as
 * the BK4819 in the Quansheng UV-K5.
 */
#include "bk4829.h"
#include "clock.h"

#define BK_PORT    GPIOE
#define BK_SCK     GPIO_PINS_10
#define BK_SDA     GPIO_PINS_11
#define BK_CS0     GPIO_PINS_8
#define BK_CS1     GPIO_PINS_15
#define BK_DELAY_US 2

static uint16_t cs_pin(bk_chip_t chip)
{
    return chip == BK_CHIP_0 ? BK_CS0 : BK_CS1;
}

static void sda_mode(bool output)
{
    gpio_init_type cfg;

    gpio_default_para_init(&cfg);
    cfg.gpio_pins = BK_SDA;
    cfg.gpio_mode = output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    cfg.gpio_pull = output ? GPIO_PULL_NONE : GPIO_PULL_UP;
    gpio_init(BK_PORT, &cfg);
}

void bk4829_bus_init(void)
{
    gpio_init_type cfg;

    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);

    BK_PORT->scr = BK_CS0 | BK_CS1;
    BK_PORT->clr = BK_SCK | BK_SDA;

    gpio_default_para_init(&cfg);
    cfg.gpio_pins = BK_SCK | BK_SDA | BK_CS0 | BK_CS1;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(BK_PORT, &cfg);
}

static void write_bits(uint32_t value, unsigned bits)
{
    for (uint32_t mask = 1U << (bits - 1); mask; mask >>= 1) {
        BK_PORT->clr = BK_SCK;
        /* One store: the low half of SCR sets pins, the high half clears them */
        BK_PORT->scr = (value & mask) ? BK_SDA : ((uint32_t)BK_SDA << 16);
        clock_delay_us(BK_DELAY_US);
        BK_PORT->scr = BK_SCK;
        clock_delay_us(BK_DELAY_US);
    }
}

void bk4829_write(bk_chip_t chip, uint8_t reg, uint16_t value)
{
    uint16_t cs = cs_pin(chip);

    BK_PORT->clr = cs;
    clock_delay_us(BK_DELAY_US);
    write_bits(reg, 8);
    write_bits(value, 16);
    clock_delay_us(BK_DELAY_US);
    BK_PORT->scr = cs;
    clock_delay_us(BK_DELAY_US);
    BK_PORT->clr = BK_SCK;
}

uint16_t bk4829_read(bk_chip_t chip, uint8_t reg)
{
    uint16_t cs = cs_pin(chip);
    uint16_t value = 0;

    BK_PORT->clr = cs;
    clock_delay_us(BK_DELAY_US);
    write_bits(reg | 0x80U, 8);
    BK_PORT->clr = BK_SCK;
    clock_delay_us(BK_DELAY_US);
    BK_PORT->scr = BK_SDA;
    sda_mode(false);
    clock_delay_us(2 * BK_DELAY_US);

    for (unsigned i = 0; i < 16; i++) {
        BK_PORT->scr = BK_SCK;
        value = (uint16_t)((value << 1) | ((BK_PORT->idt & BK_SDA) ? 1U : 0U));
        clock_delay_us(BK_DELAY_US);
        BK_PORT->clr = BK_SCK;
        clock_delay_us(BK_DELAY_US);
    }

    clock_delay_us(BK_DELAY_US);
    sda_mode(true);
    BK_PORT->scr = cs;
    clock_delay_us(BK_DELAY_US);
    BK_PORT->clr = BK_SDA | BK_SCK;
    return value;
}

/* Register, value pairs from the OEM BK4829_Init, after the soft reset */
static const uint16_t init_regs[][2] = {
    { 0x37, 0x9D1F }, { 0x13, 0x03DF }, { 0x12, 0x03DB }, { 0x11, 0x033A },
    { 0x10, 0x0318 }, { 0x14, 0x0210 }, { 0x49, 0x2AB2 }, { 0x7B, 0x73DC },
    { 0x1C, 0x07C0 }, { 0x1D, 0xE555 }, { 0x1E, 0x4C58 }, { 0x1F, 0x865A },
    { 0x3E, 0x94C6 }, { 0x3F, 0x07FE }, { 0x25, 0xC1BA }, { 0x3A, 0x9A7C },
    { 0x19, 0x1041 }, { 0x28, 0x0B40 }, { 0x29, 0xAA00 }, { 0x2A, 0x6600 },
    { 0x2C, 0x0022 }, { 0x2F, 0x9890 }, { 0x53, 0x2028 }, { 0x7E, 0x303E },
    { 0x46, 0x6050 }, { 0x4A, 0x5430 }, { 0x48, 0xB3BF }, { 0x49, 0x2AB2 },
    { 0x4A, 0x5430 }, { 0x4D, 0xA004 }, { 0x4E, 0x3815 }, { 0x4F, 0x3F3B },
    { 0x77, 0xCCEF }, { 0x7E, 0x303E },
};

/* DTMF/tone coefficient table written through register 0x09 */
static const uint16_t reg09_table[] = {
    0x006F, 0x106B, 0x2067, 0x3062, 0x4050, 0x5047, 0x603A, 0x702C,
    0x8041, 0x9037, 0xA025, 0xB017, 0xC0E4, 0xD0CB, 0xE0B5, 0xF09F,
};

static const uint16_t tail_regs[][2] = {
    { 0x74, 0xE61C }, { 0x44, 0x8F88 }, { 0x45, 0x3201 }, { 0x75, 0xE61C },
    { 0x54, 0x91C1 }, { 0x55, 0x3040 },
};

void bk4829_init(bk_chip_t chip)
{
    bk4829_write(chip, 0x00, 0x8000);
    bk4829_write(chip, 0x00, 0x0000);

    for (unsigned i = 0; i < sizeof(init_regs) / sizeof(init_regs[0]); i++) {
        bk4829_write(chip, (uint8_t)init_regs[i][0], init_regs[i][1]);
    }

    uint16_t r40 = bk4829_read(chip, 0x40);
    bk4829_write(chip, 0x40, (uint16_t)((r40 & 0xF000) | 0x04D2));
    bk4829_write(chip, 0x7D, 0xE912);
    bk4829_write(chip, 0x48, 0xB3FF);

    for (unsigned i = 0; i < sizeof(reg09_table) / sizeof(reg09_table[0]); i++) {
        bk4829_write(chip, 0x09, reg09_table[i]);
    }

    for (unsigned i = 0; i < sizeof(tail_regs) / sizeof(tail_regs[0]); i++) {
        bk4829_write(chip, (uint8_t)tail_regs[i][0], tail_regs[i][1]);
    }

    /*
     * The OEM init ends with an AF output setter (0x0801eef8) called with mode
     * 0: register 0x47 gets the AF mode from a table at 0x0802c254 (0x0000 for
     * mode 0, so muted) OR 0x6042. Register 0x48 then gets a volume from the
     * settings; we keep the 0xB3FF written above.
     */
    bk4829_write(chip, 0x47, 0x6042);
}

void bk4829_sleep(bk_chip_t chip)
{
    bk4829_write(chip, 0x30, 0x0000);
    bk4829_write(chip, 0x37, 0x1D00);
}

void bk4829_wake(bk_chip_t chip)
{
    bk4829_write(chip, 0x37, 0x9D1F);
}

/*
 * Register 0x30 values from the OEM mode setter (0x0801ed10): 0 is idle and
 * 0xBFF1 is receive (VCO, RX link, AF DAC, discriminator, PLL and RX DSP on;
 * PA gain, mic ADC and TX DSP off). The OEM writes 0x0200 first on the way
 * into receive and transmit. 0xC1FE is transmit and is only compiled in with
 * RT950_ENABLE_TX.
 */
#define REG30_IDLE     0x0000
#define REG30_RX_PREP  0x0200
#define REG30_RX       0xBFF1
#define REG30_TX       0xC1FE

void bk4829_rx_on(bk_chip_t chip, uint32_t freq)
{
    bk4829_write(chip, 0x37, 0x9F1F);
    bk4829_write(chip, 0x30, REG30_IDLE);

    /* OEM tuning routine (0x0801e5cc): frequency in 10 Hz units, then the IF filter */
    bk4829_write(chip, 0x38, (uint16_t)(freq & 0xFFFF));
    bk4829_write(chip, 0x39, (uint16_t)(freq >> 16));
    bk4829_write(chip, 0x43, 0x3028);

    /* 0x0801f060 without scrambler: clear 0x31 bit 1 and set 0x40 */
    bk4829_write(chip, 0x31, (uint16_t)(bk4829_read(chip, 0x31) & ~0x0002U));
    bk4829_write(chip, 0x40, (uint16_t)((bk4829_read(chip, 0x40) & 0xF000) | 0x04E0));

    bk4829_write(chip, 0x30, REG30_RX_PREP);
    bk4829_write(chip, 0x30, REG30_RX);
    bk4829_af(chip, false);
}

#ifdef RT950_ENABLE_TX
void bk4829_tx_on(bk_chip_t chip, uint32_t freq, uint16_t pa_bias)
{
    bk4829_write(chip, 0x37, 0x9D1F);
    bk4829_write(chip, 0x30, REG30_IDLE);
    bk4829_write(chip, 0x47, 0x6142);

    bk4829_write(chip, 0x38, (uint16_t)(freq & 0xFFFF));
    bk4829_write(chip, 0x39, (uint16_t)(freq >> 16));
    bk4829_write(chip, 0x43, 0x3028);

    /* 0x0801ebdc: clear 0x30 bit 2 */
    bk4829_write(chip, 0x30, (uint16_t)(bk4829_read(chip, 0x30) & ~0x0004U));
    /* No CTCSS/DCS (BK4819 practice: 0x51 = 0) */
    bk4829_write(chip, 0x51, 0x0000);
    /* Deviation, as for receive (0x0801f060 without scrambler) */
    bk4829_write(chip, 0x31, (uint16_t)(bk4829_read(chip, 0x31) & ~0x0002U));
    bk4829_write(chip, 0x40, (uint16_t)((bk4829_read(chip, 0x40) & 0xF000) | 0x04E0));

    bk4829_write(chip, 0x30, REG30_RX_PREP);
    bk4829_write(chip, 0x30, REG30_TX);
    /* 0x0801f02c */
    bk4829_write(chip, 0x36, pa_bias);
}
#endif

void bk4829_af(bk_chip_t chip, bool on)
{
    /* AF mode table at 0x0802c254: 0x0000 mute, 0x0100 FM; ORed with 0x6042 */
    bk4829_write(chip, 0x47, on ? 0x6142 : 0x6042);
}

int16_t bk4829_rssi_dbm(bk_chip_t chip)
{
    return (int16_t)((bk4829_read(chip, 0x67) & 0x01FF) / 2 - 160);
}
