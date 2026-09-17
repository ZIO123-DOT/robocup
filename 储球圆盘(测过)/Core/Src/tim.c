#include "tim.h"
#include "app_config.h"

TIM_HandleTypeDef htim8;

void MX_TIM8_Init(void)
{
    TIM_ClockConfigTypeDef clock = {0};
    TIM_MasterConfigTypeDef master = {0};
    TIM_OC_InitTypeDef output = {0};

    htim8.Instance = TIM8;
    htim8.Init.Prescaler = 0U;
    htim8.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
    htim8.Init.Period = DISC_PWM_MAX;
    htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim8.Init.RepetitionCounter = 0U;
    htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim8) != HAL_OK) {
        Error_Handler();
    }

    clock.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim8, &clock) != HAL_OK) {
        Error_Handler();
    }
    if (HAL_TIM_PWM_Init(&htim8) != HAL_OK) {
        Error_Handler();
    }

    master.MasterOutputTrigger = TIM_TRGO_RESET;
    master.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &master) != HAL_OK) {
        Error_Handler();
    }

    output.OCMode = TIM_OCMODE_PWM1;
    output.Pulse = 0U;
    output.OCPolarity = TIM_OCPOLARITY_HIGH;
    output.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    output.OCFastMode = TIM_OCFAST_DISABLE;
    output.OCIdleState = TIM_OCIDLESTATE_RESET;
    output.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim8, &output, TIM_CHANNEL_2) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&htim8, &output, TIM_CHANNEL_3) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(&htim8, &output, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }

    HAL_TIM_MspPostInit(&htim8);
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *timer)
{
    if (timer->Instance == TIM8) {
        __HAL_RCC_TIM8_CLK_ENABLE();
    }
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *timer)
{
    GPIO_InitTypeDef init = {0};

    if (timer->Instance != TIM8) {
        return;
    }

    __HAL_RCC_GPIOC_CLK_ENABLE();
    init.Pin = DISC_PWM_U_PIN | DISC_PWM_V_PIN | DISC_PWM_W_PIN;
    init.Mode = GPIO_MODE_AF_PP;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOC, &init);
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *timer)
{
    if (timer->Instance == TIM8) {
        __HAL_RCC_TIM8_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC,
                        DISC_PWM_U_PIN | DISC_PWM_V_PIN | DISC_PWM_W_PIN);
    }
}
