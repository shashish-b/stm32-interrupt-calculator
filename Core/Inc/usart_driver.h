/**
 * @file    usart_driver.h
 * @brief   Interrupt-driven USART2 driver for STM32F446RE (register-level / bare-metal).
 *
 * USART2 maps to PA2 (TX) / PA3 (RX), which is the pair wired to the
 * ST-LINK virtual COM port on the Nucleo-F446RE board, and to the same
 * pins in the Wokwi STM32 Nucleo simulation profile.
 *
 * Reception is fully interrupt-driven (RXNE IRQ) and buffered into a
 * lock-free single-producer/single-consumer ring buffer. Transmission
 * is interrupt-driven (TXE IRQ) as well, so the CPU never blocks
 * waiting on the UART peripheral.
 */

#ifndef USART_DRIVER_H
#define USART_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------- */
#define USART_RX_BUFFER_SIZE   128u   /* must be a power of 2 */
#define USART_TX_BUFFER_SIZE   128u   /* must be a power of 2 */
#define USART2_BAUDRATE        115200u

/**
 * @brief Initialize GPIO (PA2/PA3 AF7), USART2 peripheral, and NVIC
 *        for interrupt-driven RX/TX at USART2_BAUDRATE.
 *
 * Assumes SystemClock has already been configured (see main.c). Uses
 * APB1 peripheral clock for USART2 baud-rate calculation.
 */
void USART2_Init(void);

/**
 * @brief Queue a single byte for transmission (non-blocking).
 *        Enables TXE interrupt to drain the TX ring buffer.
 */
void USART2_WriteByte(uint8_t byte);

/**
 * @brief Queue a null-terminated string for transmission (non-blocking).
 */
void USART2_WriteString(const char *str);

/**
 * @brief Pop one byte from the RX ring buffer.
 * @param out_byte  Destination for the popped byte.
 * @return true if a byte was available and copied to *out_byte,
 *         false if the RX buffer was empty.
 */
bool USART2_ReadByte(uint8_t *out_byte);

/**
 * @brief Number of bytes currently waiting in the RX ring buffer.
 */
uint16_t USART2_RxAvailable(void);

/**
 * @brief USART2 global interrupt handler.
 *        Must be called from USART2_IRQHandler() in the vector table.
 */
void USART2_IRQHandler_Impl(void);

#endif /* USART_DRIVER_H */
