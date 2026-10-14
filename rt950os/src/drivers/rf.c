/*
 * RF path and audio output control for receive.
 *
 * Pins and order of operations follow the OEM firmware:
 * - 0x0801d704 drives the RX and TX path selects by band (see board.h).
 * - 0x0801da3e wakes the BK4829 for the selected band (0x0801d588) and puts
 *   the other to sleep (0x0801d556) before 0x0801ebf8 starts receive.
 * - 0x08022d98 switches the audio output enables.
 *
 * Transmit is only compiled in with RT950_ENABLE_TX (cmake -DRT950_TX=ON).
 * Without it the TX path selects are set up as outputs, held low and never
 * written again, and the PA drive DAC stays at 0.
 */
#include "rf.h"
#include "board.h"
#include "clock.h"

static bk_chip_t active_chip;

rf_band_t rf_band(uint32_t freq)
{
    if (freq < 10000000U) {
        return RF_BAND_LOW;
    }
    if (freq < 30000000U) {
        return RF_BAND_VHF;
    }
    return RF_BAND_UHF;
}

bk_chip_t rf_chip(uint32_t freq)
{
    return rf_band(freq) == RF_BAND_VHF ? BK_CHIP_0 : BK_CHIP_1;
}

static void output_low(gpio_type *port, uint16_t pins)
{
    gpio_init_type cfg;

    port->clr = pins;
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = pins;
    cfg.gpio_mode = GPIO_MODE_OUTPUT;
    cfg.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    cfg.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    cfg.gpio_pull = GPIO_PULL_NONE;
    gpio_init(port, &cfg);
}

/* PA drive DAC (channel 2 on PA5) enabled at 0, as when the stock firmware is not transmitting */
static void apc_init(void)
{
    gpio_init_type cfg;

    crm_periph_clock_enable(CRM_GPIOA_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DAC_PERIPH_CLOCK, TRUE);
    gpio_default_para_init(&cfg);
    cfg.gpio_pins = BOARD_APC_PIN;
    cfg.gpio_mode = GPIO_MODE_ANALOG;
    gpio_init(BOARD_APC_PORT, &cfg);
    DAC->d2dth8r = 0;
    dac_enable(DAC2_SELECT, TRUE);
}

void rf_init(void)
{
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOC_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOE_PERIPH_CLOCK, TRUE);

    output_low(BOARD_TX_VHF_PORT, BOARD_TX_VHF_PIN);
    output_low(BOARD_TX_UHF_PORT, BOARD_TX_UHF_PIN);
    output_low(BOARD_TX_LOW_PORT, BOARD_TX_LOW_PIN);
    output_low(BOARD_RX_VHF_PORT, BOARD_RX_VHF_PIN);
    output_low(BOARD_RX_UHF_PORT, BOARD_RX_UHF_PIN);
    output_low(BOARD_RX_LOW_PORT, BOARD_RX_LOW_PIN);
    output_low(BOARD_AUDIO_EN_PORT, BOARD_AUDIO_EN_PIN);
    output_low(BOARD_AUDIO_EN2_PORT, BOARD_AUDIO_EN2_PIN);
    output_low(BOARD_AUDIO_EN3_PORT, BOARD_AUDIO_EN3_PIN);
    apc_init();
}

static void rx_path_off(void)
{
    BOARD_RX_VHF_PORT->clr = BOARD_RX_VHF_PIN;
    BOARD_RX_UHF_PORT->clr = BOARD_RX_UHF_PIN;
    BOARD_RX_LOW_PORT->clr = BOARD_RX_LOW_PIN;
}

static void rx_path(rf_band_t band)
{
    rx_path_off();

    switch (band) {
    case RF_BAND_VHF: BOARD_RX_VHF_PORT->scr = BOARD_RX_VHF_PIN; break;
    case RF_BAND_UHF: BOARD_RX_UHF_PORT->scr = BOARD_RX_UHF_PIN; break;
    case RF_BAND_LOW: BOARD_RX_LOW_PORT->scr = BOARD_RX_LOW_PIN; break;
    }
}

void rf_rx_start(uint32_t freq)
{
    bk_chip_t chip = rf_chip(freq);

    rf_audio(false);
    bk4829_wake(chip);
    bk4829_sleep(chip == BK_CHIP_0 ? BK_CHIP_1 : BK_CHIP_0);
    active_chip = chip;
    bk4829_rx_on(chip, freq);
    rx_path(rf_band(freq));
}

void rf_audio(bool on)
{
    bk4829_af(active_chip, on);
    if (on) {
        BOARD_AUDIO_EN_PORT->scr = BOARD_AUDIO_EN_PIN;
        BOARD_AUDIO_EN2_PORT->scr = BOARD_AUDIO_EN2_PIN;
    } else {
        BOARD_AUDIO_EN_PORT->clr = BOARD_AUDIO_EN_PIN;
        BOARD_AUDIO_EN2_PORT->clr = BOARD_AUDIO_EN2_PIN;
        BOARD_AUDIO_EN3_PORT->clr = BOARD_AUDIO_EN3_PIN;
    }
}

#ifdef RT950_ENABLE_TX
/*
 * PA drive for the first transmit tests: 0, so the PA gets no drive and only
 * a few milliwatts at most leave the antenna socket. The stock firmware takes
 * this byte from its calibration tables in RAM (0x0801ea14), through pointer
 * tables we have not located in flash yet. Raise it only after reading those
 * tables from a running stock radio over SWD, and with a dummy load.
 */
#define TX_APC_LEVEL 0

/* Register 0x36 for the lowest stock power level (table at 0x0802c16c, index 2) */
#define TX_PA_BIAS 0x0057

static bool tx_allowed(uint32_t freq)
{
    return (freq >= 14400000U && freq <= 14600000U) || (freq >= 43000000U && freq <= 44000000U);
}

bool rf_tx_start(uint32_t freq)
{
    bk_chip_t chip = rf_chip(freq);

    if (!tx_allowed(freq)) {
        return false;
    }

    /* OEM 0x0801da6c: RF paths off, BK4829 into TX, PA drive, then the TX path select */
    rf_audio(false);
    rx_path_off();
    bk4829_wake(chip);
    bk4829_sleep(chip == BK_CHIP_0 ? BK_CHIP_1 : BK_CHIP_0);
    active_chip = chip;
    bk4829_tx_on(chip, freq, TX_PA_BIAS);
    DAC->d2dth8r = TX_APC_LEVEL;

    if (rf_band(freq) == RF_BAND_VHF) {
        BOARD_TX_VHF_PORT->scr = BOARD_TX_VHF_PIN;
    } else {
        BOARD_TX_UHF_PORT->scr = BOARD_TX_UHF_PIN;
    }
    return true;
}

void rf_tx_stop(uint32_t rx_freq)
{
    /* OEM 0x08024a86: PA drive off, 50 ms, then the BK4829 out of TX */
    DAC->d2dth8r = 0;
    clock_delay_ms(50);
    BOARD_TX_VHF_PORT->clr = BOARD_TX_VHF_PIN;
    BOARD_TX_UHF_PORT->clr = BOARD_TX_UHF_PIN;
    BOARD_TX_LOW_PORT->clr = BOARD_TX_LOW_PIN;
    bk4829_write(active_chip, 0x30, 0x0000);
    rf_rx_start(rx_freq);
}
#endif
