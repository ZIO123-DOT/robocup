# STM32CubeMX 配置步骤

## 1. MCU 与时钟

1. 选择 `STM32F103ZET6`，LQFP144。
2. SYS → Debug 选择 `Serial Wire`，保留 PA13/PA14。
3. RCC 选择 HSE Crystal/Ceramic Resonator。
4. HSE=8 MHz，PLL×9，SYSCLK=72 MHz，APB1=36 MHz，APB2=72 MHz。

## 2. TIM8 三路 PWM

启用：

- PC7：TIM8_CH2 → PWM Generation CH2；
- PC8：TIM8_CH3 → PWM Generation CH3；
- PC9：TIM8_CH4 → PWM Generation CH4。

参数：

- Prescaler：0；
- Counter Mode：Center Aligned mode 1；
- Counter Period：1799；
- Auto-reload preload：Enable；
- 三个通道初始 Pulse：0；
- 极性：High。

## 3. AS5600 PWM

PA1 设置为 GPIO_Input、No pull。程序使用 Cortex-M3 DWT 周期计数器测量 AS5600 PWM 的高电平和完整周期，不额外占用输入捕获定时器。

如果 AS5600 PWM 为 5 V，必须先降到 3.3 V。

## 4. USART1

- PA9：USART1_TX；PA10：USART1_RX；
- Asynchronous，115200、8 data bits、1 stop bit、No parity；
- 打开 USART1 global interrupt。

## 5. 工程生成

- Project Name：`storage_disc`
- Toolchain/IDE：MDK-ARM V5
- Firmware：STM32Cube FW_F1 V1.8.7

本目录已经包含完整 CubeMX 与 Keil 工程。重新生成代码前必须备份 `main.c`、`tim.c`、`gpio.c`、`storage_disc.c/.h` 和 `app_config.h`。
