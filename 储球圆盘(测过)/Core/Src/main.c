#include "main.h"
#include "gpio.h"
#include "tim.h"
#include "usart.h"
#include "app_config.h"
#include "storage_disc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void SystemClock_Config(void);

static uint8_t uart_rx_byte;
static volatile uint8_t uart_rx_head;
static volatile uint8_t uart_rx_tail;
static volatile uint8_t uart_rx_ring[UART_RX_RING_SIZE];
static uint8_t stop_match;

static void uart_text(const char *text)
{
    (void)HAL_UART_Transmit(&huart1,
                            (uint8_t *)text,
                            (uint16_t)strlen(text),
                            500U);
}

static uint8_t uart_get_byte(uint8_t *byte)
{
    uint8_t tail = uart_rx_tail;

    if (tail == uart_rx_head) {
        return 0U;
    }
    *byte = uart_rx_ring[tail];
    uart_rx_tail = (uint8_t)((tail + 1U) % UART_RX_RING_SIZE);
    return 1U;
}

static uint8_t uart_read_command(char *buffer, uint8_t size)
{
    uint8_t ch;
    uint8_t length = 0U;
    uint32_t last_byte_ms = HAL_GetTick();

    if (buffer == NULL || size < 2U) {
        return 0U;
    }

    while (1) {
        if (uart_get_byte(&ch) != 0U) {
            last_byte_ms = HAL_GetTick();
            if (ch == '\r' || ch == '\n') {
                if (length != 0U) {
                    buffer[length] = '\0';
                    return 1U;
                }
            } else if (ch >= 32U && ch <= 126U && length < size - 1U) {
                if (ch >= 'A' && ch <= 'Z') {
                    ch = (uint8_t)(ch - 'A' + 'a');
                }
                buffer[length++] = (char)ch;
            }
        } else if (length != 0U &&
                   (uint32_t)(HAL_GetTick() - last_byte_ms) >=
                   UART_COMMAND_IDLE_MS) {
            buffer[length] = '\0';
            return 1U;
        } else {
            HAL_Delay(1U);
        }
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    uint8_t next;
    uint8_t lower = uart_rx_byte;

    if (uart->Instance != USART1) {
        return;
    }

    next = (uint8_t)((uart_rx_head + 1U) % UART_RX_RING_SIZE);
    if (next != uart_rx_tail) {
        uart_rx_ring[uart_rx_head] = uart_rx_byte;
        uart_rx_head = next;
    }

    if (lower >= 'A' && lower <= 'Z') {
        lower = (uint8_t)(lower - 'A' + 'a');
    }
    if (lower == (uint8_t)"stop"[stop_match]) {
        stop_match++;
        if (stop_match == 4U) {
            StorageDisc_Stop();
            stop_match = 0U;
        }
    } else {
        stop_match = (uint8_t)(lower == 's');
    }

    (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart->Instance == USART1) {
        (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U);
    }
}

static uint8_t parse_angle(const char *command, uint16_t *degree)
{
    const char *number = command;
    char *end;
    unsigned long value;

    if (strncmp(command, "angle", 5U) == 0) {
        number = command + 5U;
    }
    while (*number == ' ') {
        number++;
    }
    if (*number < '0' || *number > '9') {
        return 0U;
    }

    value = strtoul(number, &end, 10);
    while (*end == ' ') {
        end++;
    }
    if (*end != '\0' || value > 360UL) {
        return 0U;
    }
    *degree = (uint16_t)value;
    return 1U;
}

static uint8_t parse_jog(const char *command,
                         const char *prefix,
                         uint16_t *cycles)
{
    const char *number;
    char *end;
    unsigned long value;
    size_t length = strlen(prefix);

    if (strncmp(command, prefix, length) != 0) {
        return 0U;
    }
    number = command + length;
    if (*number == '\0') {
        *cycles = 1U;
        return 1U;
    }
    value = strtoul(number, &end, 10);
    if (*end != '\0' || value == 0UL || value > DISC_MAX_JOG_CYCLES) {
        return 0U;
    }
    *cycles = (uint16_t)value;
    return 1U;
}

static void print_angle(void)
{
    int32_t angle10 = StorageDisc_ReadAngle10();
    char line[64];

    if (angle10 < 0) {
        uart_text("AS5600 FAIL: no valid PWM\r\n");
        return;
    }
    sprintf(line,
            "ANGLE=%ld.%ld deg\r\n",
            (long)(angle10 / 10),
            (long)(angle10 % 10));
    uart_text(line);
}

static void print_help(void)
{
    uart_text("Commands:\r\n");
    uart_text("  angle0..angle360 or 0..360 = closed-loop position\r\n");
    uart_text("  read = print AS5600 angle\r\n");
    uart_text("  cwN / ccwN = open-loop electrical cycles, N=1..20\r\n");
    uart_text("  stop = immediate PWM off\r\n");
    uart_text("  help\r\n");
}

int main(void)
{
    char command[40];
    char line[80];
    uint16_t value;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM8_Init();
    MX_USART1_UART_Init();
    StorageDisc_Init();
    StorageDisc_Stop();
    (void)HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1U);

    uart_text("\r\n=== STORAGE DISC READY ===\r\n");
    uart_text("Power-on state: STOP. Send read/help first.\r\n");

    while (1) {
        if (uart_read_command(command, sizeof(command)) == 0U) {
            continue;
        }
        sprintf(line, "RX: %s\r\n", command);
        uart_text(line);

        if (strcmp(command, "read") == 0) {
            print_angle();
        } else if (strcmp(command, "stop") == 0) {
            StorageDisc_Stop();
            uart_text("STOP OK\r\n");
        } else if (strcmp(command, "help") == 0) {
            print_help();
        } else if (parse_jog(command, "ccw", &value) != 0U) {
            uart_text(StorageDisc_Jog(1U, value) != 0U ?
                      "CCW OK\r\n" : "CCW STOP/FAIL\r\n");
        } else if (parse_jog(command, "cw", &value) != 0U) {
            uart_text(StorageDisc_Jog(0U, value) != 0U ?
                      "CW OK\r\n" : "CW STOP/FAIL\r\n");
        } else if (parse_angle(command, &value) != 0U) {
            sprintf(line, "TARGET=%u deg\r\n", value);
            uart_text(line);
            if (StorageDisc_MoveTo(value) != 0U) {
                uart_text("POSITION OK\r\n");
                print_angle();
            } else {
                sprintf(line,
                        "POSITION FAIL: %s\r\n",
                        StorageDisc_LastStatus());
                uart_text(line);
            }
        } else {
            uart_text("ERR: send help\r\n");
        }
    }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator = {0};
    RCC_ClkInitTypeDef clocks = {0};

    oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    oscillator.HSEState = RCC_HSE_ON;
    oscillator.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    oscillator.HSIState = RCC_HSI_ON;
    oscillator.PLL.PLLState = RCC_PLL_ON;
    oscillator.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    oscillator.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
        Error_Handler();
    }

    clocks.ClockType = RCC_CLOCKTYPE_HCLK |
                       RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 |
                       RCC_CLOCKTYPE_PCLK2;
    clocks.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clocks.APB1CLKDivider = RCC_HCLK_DIV2;
    clocks.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
