.syntax unified
.cpu cortex-m3
.thumb

.global g_pfnVectors
.global Reset_Handler

.section .isr_vector,"a",%progbits
.type g_pfnVectors, %object
g_pfnVectors:
    .word _estack
    .word Reset_Handler
    .word Default_Handler       /* NMI */
    .word Default_Handler       /* HardFault */
    .word Default_Handler       /* MemManage */
    .word Default_Handler       /* BusFault */
    .word Default_Handler       /* UsageFault */
    .word 0
    .word 0
    .word 0
    .word 0
    .word Default_Handler       /* SVCall */
    .word Default_Handler       /* DebugMon */
    .word 0
    .word Default_Handler       /* PendSV */
    .word SysTick_Handler

    /* IRQs externas STM32F103C8. */
    .word Default_Handler       /* 0  WWDG */
    .word Default_Handler       /* 1  PVD */
    .word Default_Handler       /* 2  TAMPER */
    .word Default_Handler       /* 3  RTC */
    .word Default_Handler       /* 4  FLASH */
    .word Default_Handler       /* 5  RCC */
    .word Default_Handler       /* 6  EXTI0 */
    .word Default_Handler       /* 7  EXTI1 */
    .word Default_Handler       /* 8  EXTI2 */
    .word Default_Handler       /* 9  EXTI3 */
    .word Default_Handler       /* 10 EXTI4 */
    .word Default_Handler       /* 11 DMA1_Channel1 */
    .word Default_Handler       /* 12 DMA1_Channel2 */
    .word Default_Handler       /* 13 DMA1_Channel3 */
    .word Default_Handler       /* 14 DMA1_Channel4 */
    .word Default_Handler       /* 15 DMA1_Channel5 */
    .word Default_Handler       /* 16 DMA1_Channel6 */
    .word Default_Handler       /* 17 DMA1_Channel7 */
    .word Default_Handler       /* 18 ADC1_2 */
    .word Default_Handler       /* 19 USB_HP_CAN1_TX */
    .word Default_Handler       /* 20 USB_LP_CAN1_RX0 */
    .word Default_Handler       /* 21 CAN1_RX1 */
    .word Default_Handler       /* 22 CAN1_SCE */
    .word EXTI9_5_IRQHandler    /* 23 EXTI9_5 */
    .word Default_Handler       /* 24 TIM1_BRK */
    .word Default_Handler       /* 25 TIM1_UP */
    .word Default_Handler       /* 26 TIM1_TRG_COM */
    .word Default_Handler       /* 27 TIM1_CC */
    .word Default_Handler       /* 28 TIM2 */
    .word Default_Handler       /* 29 TIM3 */
    .word Default_Handler       /* 30 TIM4 */
    .word Default_Handler       /* 31 I2C1_EV */
    .word Default_Handler       /* 32 I2C1_ER */
    .word Default_Handler       /* 33 I2C2_EV */
    .word Default_Handler       /* 34 I2C2_ER */
    .word Default_Handler       /* 35 SPI1 */
    .word Default_Handler       /* 36 SPI2 */
    .word USART1_IRQHandler     /* 37 USART1 */
    .word USART2_IRQHandler     /* 38 USART2 */
    .word Default_Handler       /* 39 USART3 */
    .word Default_Handler       /* 40 EXTI15_10 */
    .word Default_Handler       /* 41 RTCAlarm */
    .word Default_Handler       /* 42 USBWakeUp */
.size g_pfnVectors, .-g_pfnVectors

.section .text.Reset_Handler,"ax",%progbits
.type Reset_Handler, %function
.thumb_func
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
1:
    cmp r1, r2
    bcc 2f
    b 3f
2:
    ldr r3, [r0], #4
    str r3, [r1], #4
    b 1b
3:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
4:
    cmp r1, r2
    bcc 5f
    b 6f
5:
    str r3, [r1], #4
    b 4b
6:
    bl main
7:
    b 7b
.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler,"ax",%progbits
.type Default_Handler, %function
.thumb_func
Default_Handler:
    b Default_Handler
.size Default_Handler, .-Default_Handler

.weak SysTick_Handler
.thumb_set SysTick_Handler, Default_Handler
.weak EXTI9_5_IRQHandler
.thumb_set EXTI9_5_IRQHandler, Default_Handler
.weak USART1_IRQHandler
.thumb_set USART1_IRQHandler, Default_Handler
.weak USART2_IRQHandler
.thumb_set USART2_IRQHandler, Default_Handler
