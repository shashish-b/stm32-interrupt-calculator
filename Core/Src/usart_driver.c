/**
 * @file    usart_driver.c
 * @brief   Register-level USART2 driver implementation (STM32F446RE).
 */

#include "usart_driver.h"
#include "stm32f4xx.h"   /* CMSIS device header: USART2, GPIOA, RCC, NVIC */

/* ---------------------------------------------------------------------
 * Ring buffers (single-producer/single-consumer)
 * ------------------------------------------------------------------- */
static volatile uint8_t  rxBuffer[USART_RX_BUFFER_SIZE];
static volatile uint16_t rxHead = 0;   /* written by ISR (producer)        */
static volatile uint16_t rxTail = 0;   /* written by main loop (consumer)  */

static volatile uint8_t  txBuffer[USART_TX_BUFFER_SIZE];
static volatile uint16_t txHead = 0;   /* written by main loop (producer)  */
static volatile uint16_t txTail = 0;   /* written by ISR (consumer)        */

#define RX_MASK (USART_RX_BUFFER_SIZE - 1u)
#define TX_MASK (USART_TX_BUFFER_SIZE - 1u)

/* ---------------------------------------------------------------------
 * Init: GPIO AF7 (PA2=TX, PA3=RX), USART2 8N1, RXNE interrupt enabled
 * ------------------------------------------------------------------- */
void USART2_Init(void)
{
    /* 1. Clock enable: GPIOA and USART2 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* 2. Configure PA2 (TX) and PA3 (RX) as alternate function */
    GPIOA->MODER &= ~(GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOA->MODER |=  (GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1); /* AF mode = 10 */

    /* AF7 = USART2 on PA2/PA3, set in AFR[0] (covers pins 0-7) */
    GPIOA->AFR[0] &= ~(0xFu << (2 * 4));
    GPIOA->AFR[0] |=  (0x7u << (2 * 4));   /* PA2 -> AF7 */
    GPIOA->AFR[0] &= ~(0xFu << (3 * 4));
    GPIOA->AFR[0] |=  (0x7u << (3 * 4));   /* PA3 -> AF7 */

    /* Push-pull, no pull-up/down, high speed (typical UART TX config) */
    GPIOA->OTYPER  &= ~(GPIO_OTYPER_OT_2 | GPIO_OTYPER_OT_3);
    GPIOA->PUPDR   &= ~(GPIO_PUPDR_PUPD2 | GPIO_PUPDR_PUPD3);
    GPIOA->OSPEEDR |=  (GPIO_OSPEEDR_OSPEED2 | GPIO_OSPEEDR_OSPEED3);

    /* 3. USART2 configuration: 8 data bits, 1 stop bit, no parity */
    USART2->CR1 = 0;
    USART2->CR2 = 0;
    USART2->CR3 = 0;

    /* Baud rate: USARTDIV = f_PCLK1 / (16 * baud) for oversampling by 16.
         * This project's SystemClock_Config() (CubeIDE-generated) leaves the
         * MCU running on raw HSI with no PLL and no APB1 prescaler, so
         * SYSCLK = HCLK = PCLK1 = 16 MHz. If SystemClock_Config() is ever
         * changed to enable the PLL or a different APB1 prescaler, this
         * value must be updated to match. */
        const uint32_t pclk1 = 16000000u;
        uint32_t usartdiv = (pclk1 + (USART2_BAUDRATE / 2u)) / USART2_BAUDRATE;
        USART2->BRR = (uint16_t)usartdiv;

    /* Enable RXNE interrupt, enable RX + TX, enable USART */
    USART2->CR1 |= USART_CR1_RXNEIE;
    USART2->CR1 |= USART_CR1_TE | USART_CR1_RE;
    USART2->CR1 |= USART_CR1_UE;

    /* 4. NVIC: enable USART2 interrupt line */
    NVIC_SetPriority(USART2_IRQn, 1);
    NVIC_EnableIRQ(USART2_IRQn);
}

/* ---------------------------------------------------------------------
 * TX: push into ring buffer, kick off TXE interrupt to drain it
 * ------------------------------------------------------------------- */
void USART2_WriteByte(uint8_t byte)
{
    uint16_t next = (uint16_t)((txHead + 1u) & TX_MASK);

    /* Buffer full: drop byte rather than block (non-blocking contract).
     * In practice the calculator's response strings are short and the
     * buffer drains far faster than a human types. */
    if (next == txTail) {
        return;
    }

    txBuffer[txHead] = byte;
    txHead = next;

    /* Ensure TXE interrupt is enabled so the ISR drains the buffer */
    USART2->CR1 |= USART_CR1_TXEIE;
}

void USART2_WriteString(const char *str)
{
    while (*str != '\0') {
        USART2_WriteByte((uint8_t)*str);
        str++;
    }
}

/* ---------------------------------------------------------------------
 * RX: pop from ring buffer (called from main loop, not ISR context)
 * ------------------------------------------------------------------- */
bool USART2_ReadByte(uint8_t *out_byte)
{
    if (rxTail == rxHead) {
        return false; /* empty */
    }

    *out_byte = rxBuffer[rxTail];
    rxTail = (uint16_t)((rxTail + 1u) & RX_MASK);
    return true;
}

uint16_t USART2_RxAvailable(void)
{
    return (uint16_t)((rxHead - rxTail) & RX_MASK);
}

/* ---------------------------------------------------------------------
 * ISR: handles both RXNE (byte received) and TXE (ready to transmit)
 * ------------------------------------------------------------------- */
void USART2_IRQHandler_Impl(void)
{
    /* --- Receive path --- */
    if (USART2->SR & USART_SR_RXNE) {
        uint8_t data = (uint8_t)(USART2->DR & 0xFFu); /* reading DR clears RXNE */

        uint16_t next = (uint16_t)((rxHead + 1u) & RX_MASK);
        if (next != rxTail) {           /* drop byte if buffer is full */
            rxBuffer[rxHead] = data;
            rxHead = next;
        }
    }

    /* --- Transmit path --- */
    if ((USART2->SR & USART_SR_TXE) && (USART2->CR1 & USART_CR1_TXEIE)) {
        if (txHead != txTail) {
            USART2->DR = txBuffer[txTail];  /* writing DR clears TXE */
            txTail = (uint16_t)((txTail + 1u) & TX_MASK);
        } else {
            /* Nothing left to send: disable TXE interrupt until the
             * next byte is queued, otherwise it fires continuously. */
            USART2->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}
