#include "gpio.h"
#include "app_config.h"

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef init = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* AS5600 PWM is an input to the STM32. Use a level shifter/divider if 5 V. */
    init.Pin = DISC_AS5600_PWM_PIN;
    init.Mode = GPIO_MODE_INPUT;
    init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DISC_AS5600_PWM_PORT, &init);
}
