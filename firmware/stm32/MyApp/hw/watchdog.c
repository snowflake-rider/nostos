#include "watchdog.h"

#include "stm32f4xx_hal.h"

#define WATCHDOG_WRITE_ACCESS_KEY 0x5555U
#define WATCHDOG_START_KEY 0xCCCCU
#define WATCHDOG_REFRESH_KEY 0xAAAAU
#define WATCHDOG_PRESCALER_DIV64 IWDG_PR_PR_2
#define WATCHDOG_RELOAD_VALUE 1999U
#define WATCHDOG_REGISTER_TIMEOUT_MS 100U
#define WATCHDOG_UPDATE_FLAGS (IWDG_SR_PVU | IWDG_SR_RVU)

bool watchdog_start(void)
{
    /* Starting first forces the LSI clock on. The configuration update flags
     * cannot clear reliably while that clock is stopped. */
    IWDG->KR = WATCHDOG_START_KEY;
    IWDG->KR = WATCHDOG_WRITE_ACCESS_KEY;
    IWDG->PR = WATCHDOG_PRESCALER_DIV64;
    IWDG->RLR = WATCHDOG_RELOAD_VALUE;

    uint32_t started_ms = HAL_GetTick();
    while ((IWDG->SR & WATCHDOG_UPDATE_FLAGS) != 0U)
    {
        if ((uint32_t)(HAL_GetTick() - started_ms) > WATCHDOG_REGISTER_TIMEOUT_MS)
        {
            return false;
        }
    }

    IWDG->KR = WATCHDOG_REFRESH_KEY;
    return true;
}

void watchdog_refresh(void)
{
    IWDG->KR = WATCHDOG_REFRESH_KEY;
}
