#include "clock.h"

static volatile uint32_t tick_ms;

void SysTick_Handler(void)
{
    tick_ms++;
}

/*
 * 240 MHz from the internal 8 MHz HICK (HICK/2 x 60), the same setup the
 * AT32 Workbench reference project in "reference project/radtel" uses. The
 * HICK keeps us independent of the board's crystal, which is still unknown.
 * The OEM bootloader may hand over with the PLL already running, so the CRM
 * is reset first.
 */
void clock_init(void)
{
    crm_reset();

    crm_clock_source_enable(CRM_CLOCK_SOURCE_HICK, TRUE);
    while (crm_flag_get(CRM_HICK_STABLE_FLAG) != SET) {
    }

    crm_pll_config(CRM_PLL_SOURCE_HICK, CRM_PLL_MULT_60, CRM_PLL_OUTPUT_RANGE_GT72MHZ);
    crm_clock_source_enable(CRM_CLOCK_SOURCE_PLL, TRUE);
    while (crm_flag_get(CRM_PLL_STABLE_FLAG) != SET) {
    }

    crm_ahb_div_set(CRM_AHB_DIV_1);
    crm_apb2_div_set(CRM_APB2_DIV_2);
    crm_apb1_div_set(CRM_APB1_DIV_2);

    /* Step the AHB clock up through the divider, as the vendor code does above 108 MHz */
    crm_auto_step_mode_enable(TRUE);
    crm_sysclk_switch(CRM_SCLK_PLL);
    while (crm_sysclk_switch_status_get() != CRM_SCLK_PLL) {
    }
    crm_auto_step_mode_enable(FALSE);

    system_core_clock_update();

    SysTick_Config(system_core_clock / 1000U);

    /* Cycle counter for microsecond delays */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void clock_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (system_core_clock / 1000000U);

    while ((DWT->CYCCNT - start) < cycles) {
    }
}

uint32_t clock_millis(void)
{
    return tick_ms;
}

void clock_delay_ms(uint32_t ms)
{
    uint32_t start = tick_ms;
    while ((tick_ms - start) < ms) {
    }
}
