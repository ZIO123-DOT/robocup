#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "main.h"

/* Storage-disc three-phase power-stage inputs: TIM8 CH2/CH3/CH4. */
#define DISC_PWM_U_PORT              GPIOC
#define DISC_PWM_U_PIN               GPIO_PIN_7
#define DISC_PWM_V_PORT              GPIOC
#define DISC_PWM_V_PIN               GPIO_PIN_8
#define DISC_PWM_W_PORT              GPIOC
#define DISC_PWM_W_PIN               GPIO_PIN_9

/* Second AS5600 PWM output. Read by DWT polling; no timer channel required. */
#define DISC_AS5600_PWM_PORT         GPIOA
#define DISC_AS5600_PWM_PIN          GPIO_PIN_1

#define DISC_PWM_MAX                 1799U
#define DISC_PWM_HALF                (DISC_PWM_MAX / 2U)
#define DISC_ANGLE_TOLERANCE10       30U
#define DISC_TARGET_CONFIRMATIONS    3U
#define DISC_MOVE_TIMEOUT_MS         20000U
#define DISC_SENSOR_EDGE_TIMEOUT_US  10000U
#define DISC_MAX_DIRECTION_REVERSALS 4U
#define DISC_ROTOR_ALIGN_US          0U
#define DISC_START_COMMUTATE_US      25000U
#define DISC_STALL_CHECK_MS          2500U
#define DISC_STALL_MIN_CHANGE10      10U

/* Measured on the real mechanism: CCW increases the AS5600 angle. */
#define DISC_POSITIVE_ANGLE_REVERSE  1U

/* Real motor test: only CW commutation is reliable. Position in CW only. */
#define DISC_POSITION_CW_ONLY        1U

#define UART_COMMAND_IDLE_MS         120U
#define UART_RX_RING_SIZE            64U
#define DISC_MAX_JOG_CYCLES          20U

#endif
