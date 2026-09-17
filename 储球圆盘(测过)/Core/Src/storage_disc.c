#include "storage_disc.h"
#include "app_config.h"
#include "tim.h"

static volatile uint8_t disc_abort;
static const char *last_status = "IDLE";

static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);

    while ((uint32_t)(DWT->CYCCNT - start) < ticks) { }
}

static uint8_t wait_pwm_level(GPIO_PinState level, uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t timeout = timeout_us * (SystemCoreClock / 1000000U);

    while (HAL_GPIO_ReadPin(DISC_AS5600_PWM_PORT,
                            DISC_AS5600_PWM_PIN) != level) {
        if ((uint32_t)(DWT->CYCCNT - start) >= timeout) {
            return 0U;
        }
    }
    return 1U;
}

static void motor_stop(void)
{
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0U);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 0U);
}

static void apply_phase(uint8_t phase_index, uint8_t reverse)
{
    uint16_t c = DISC_PWM_HALF;
    uint16_t d = DISC_PWM_MAX / 6U;
    uint16_t u = c;
    uint16_t v = c;
    uint16_t w = c;
    uint8_t phase = reverse != 0U ?
                    (uint8_t)((6U - phase_index) % 6U) :
                    (uint8_t)(phase_index % 6U);

    switch (phase) {
    case 0U: u = c + d; v = c - d; break;
    case 1U: u = c + d; w = c - d; break;
    case 2U: v = c + d; w = c - d; break;
    case 3U: u = c - d; v = c + d; break;
    case 4U: u = c - d; w = c + d; break;
    case 5U: v = c - d; w = c + d; break;
    default: break;
    }

    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, u);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, v);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, w);
}

static int32_t shortest_error10(int32_t target, int32_t current)
{
    int32_t error = target - current;

    while (error > 1800) {
        error -= 3600;
    }
    while (error < -1800) {
        error += 3600;
    }
    return error;
}

static int32_t cw_remaining10(int32_t target, int32_t current)
{
    int32_t remaining = current - target;

    while (remaining < 0) {
        remaining += 3600;
    }
    while (remaining >= 3600) {
        remaining -= 3600;
    }
    return remaining;
}

void StorageDisc_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    disc_abort = 0U;
    if (HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3) != HAL_OK ||
        HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4) != HAL_OK) {
        Error_Handler();
    }
    motor_stop();
    last_status = "READY";
}

void StorageDisc_Stop(void)
{
    disc_abort = 1U;
    motor_stop();
    last_status = "STOP COMMAND";
}

const char *StorageDisc_LastStatus(void)
{
    return last_status;
}

int32_t StorageDisc_ReadAngle10(void)
{
    uint32_t rise;
    uint32_t high_cycles;
    uint32_t period_cycles;
    float duty;
    float angle;

    if (wait_pwm_level(GPIO_PIN_RESET, DISC_SENSOR_EDGE_TIMEOUT_US) == 0U ||
        wait_pwm_level(GPIO_PIN_SET, DISC_SENSOR_EDGE_TIMEOUT_US) == 0U) {
        return -1;
    }

    rise = DWT->CYCCNT;
    if (wait_pwm_level(GPIO_PIN_RESET, DISC_SENSOR_EDGE_TIMEOUT_US) == 0U) {
        return -1;
    }
    high_cycles = (uint32_t)(DWT->CYCCNT - rise);
    if (wait_pwm_level(GPIO_PIN_SET, DISC_SENSOR_EDGE_TIMEOUT_US) == 0U) {
        return -1;
    }
    period_cycles = (uint32_t)(DWT->CYCCNT - rise);

    if (period_cycles == 0U || high_cycles >= period_cycles) {
        return -1;
    }

    duty = (float)high_cycles / (float)period_cycles;
    angle = (duty - 0.02f) / 0.96f * 360.0f;
    if (angle < 0.0f) {
        angle = 0.0f;
    }
    if (angle >= 360.0f) {
        angle = 0.0f;
    }
    return (int32_t)(angle * 10.0f + 0.5f);
}

uint8_t StorageDisc_Jog(uint8_t reverse, uint16_t electrical_cycles)
{
    uint16_t total_steps;
    uint16_t step_count;
    uint32_t step_delay;
    uint8_t phase;

    if (electrical_cycles == 0U ||
        electrical_cycles > DISC_MAX_JOG_CYCLES) {
        last_status = "INVALID JOG COUNT";
        return 0U;
    }

    disc_abort = 0U;
    last_status = "JOG RUNNING";

    total_steps = (uint16_t)(electrical_cycles * 6U);
    for (step_count = 0U; step_count < total_steps; step_count++) {
        if (disc_abort != 0U) {
            motor_stop();
            last_status = "STOP COMMAND";
            return 0U;
        }
        phase = (uint8_t)(step_count % 6U);
        apply_phase(phase, reverse);
        step_delay = DISC_START_COMMUTATE_US;
        delay_us(step_delay);
    }
    motor_stop();
    last_status = "JOG OK";
    return 1U;
}

uint8_t StorageDisc_MoveTo(uint16_t degree)
{
    uint32_t started_ms;
    uint32_t progress_checked_ms;
    uint32_t commutate_us;
    int32_t current;
    int32_t target;
    int32_t error;
    int32_t magnitude;
    int32_t progress_reference;
    int32_t progress_change;
    uint32_t commutation_count = 0U;
    uint8_t phase = 0U;
    uint8_t reverse;
    uint8_t last_reverse = 2U;
    uint8_t reversal_count = 0U;
    uint8_t target_count = 0U;

    if (degree > 360U) {
        last_status = "INVALID TARGET";
        return 0U;
    }

    disc_abort = 0U;
    last_status = "POSITION RUNNING";
    motor_stop();
    current = StorageDisc_ReadAngle10();
    if (current < 0 || disc_abort != 0U) {
        motor_stop();
        last_status = current < 0 ? "AS5600 PWM INVALID" : "STOP COMMAND";
        return 0U;
    }

    target = (int32_t)((degree == 360U ? 0U : degree) * 10U);

    started_ms = HAL_GetTick();
    progress_checked_ms = started_ms;
    progress_reference = current;

    while ((uint32_t)(HAL_GetTick() - started_ms) < DISC_MOVE_TIMEOUT_MS) {
        if (disc_abort != 0U) {
            motor_stop();
            last_status = "STOP COMMAND";
            return 0U;
        }

        current = StorageDisc_ReadAngle10();
        if (current < 0 || disc_abort != 0U) {
            motor_stop();
            last_status = current < 0 ? "AS5600 PWM INVALID" : "STOP COMMAND";
            return 0U;
        }

        error = shortest_error10(target, current);
        magnitude = error < 0 ? -error : error;
        if ((uint32_t)magnitude <= DISC_ANGLE_TOLERANCE10) {
            motor_stop();
            target_count++;
            if (target_count >= DISC_TARGET_CONFIRMATIONS) {
                last_status = DISC_POSITION_CW_ONLY ?
                              "POSITION OK (CW ONLY)" : "POSITION OK";
                return 1U;
            }
            HAL_Delay(10U);
            continue;
        }

        target_count = 0U;
#if DISC_POSITION_CW_ONLY
        /* CW was verified to decrease AS5600 angle; CCW stalls on this motor. */
        reverse = 0U;
        magnitude = cw_remaining10(target, current);
#else
        if (error > 0) {
            reverse = DISC_POSITIVE_ANGLE_REVERSE;
        } else {
            reverse = (uint8_t)(1U - DISC_POSITIVE_ANGLE_REVERSE);
        }
#endif
        if (last_reverse <= 1U && reverse != last_reverse) {
            reversal_count++;
            phase = 0U;
            if (reversal_count >= DISC_MAX_DIRECTION_REVERSALS) {
                motor_stop();
                last_status = "DIRECTION OSCILLATION";
                return 0U;
            }
        }
        last_reverse = reverse;

        if (commutation_count < 12U) {
            commutate_us = DISC_START_COMMUTATE_US;
        } else if (magnitude <= 100) {
            commutate_us = 30000U;
        } else if (magnitude <= 300) {
            commutate_us = 22000U;
        } else {
            commutate_us = 15000U;
        }

        apply_phase(phase, reverse);
        phase++;
        if (phase >= 6U) {
            phase = 0U;
        }
        delay_us(commutate_us);
        commutation_count++;

        if ((uint32_t)(HAL_GetTick() - progress_checked_ms) >=
            DISC_STALL_CHECK_MS) {
            progress_change = shortest_error10(current, progress_reference);
            if (progress_change < 0) progress_change = -progress_change;
            if ((uint32_t)progress_change < DISC_STALL_MIN_CHANGE10) {
                motor_stop();
                last_status = "STALL: ANGLE DID NOT CHANGE";
                return 0U;
            }
            progress_reference = current;
            progress_checked_ms = HAL_GetTick();
        }
    }

    motor_stop();
    last_status = "POSITION TIMEOUT";
    return 0U;
}
