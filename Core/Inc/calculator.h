/**
 * @file    calculator.h
 * @brief   Line-based arithmetic expression parser and evaluator.
 *
 * Accepts expressions of the form:   <number> <operator> <number>
 * e.g. "5 + 3", "12 * 4", "10 / 0"
 *
 * Supports + - * / with single-precision float operands and result.
 * Designed to run from the main loop against bytes pulled out of the
 * USART2 RX ring buffer -- it does not block and does no I/O itself,
 * it only assembles and evaluates a line of text.
 */

#ifndef CALCULATOR_H
#define CALCULATOR_H

#include <stdint.h>
#include <stdbool.h>

#define CALC_LINE_MAX_LEN   32u

typedef enum {
    CALC_STATE_IDLE,        /* waiting for first character of a new line */
    CALC_STATE_COLLECTING,  /* accumulating characters until CR/LF       */
    CALC_STATE_READY        /* full line collected, ready to evaluate    */
} calc_state_t;

typedef enum {
    CALC_OK = 0,
    CALC_ERR_SYNTAX,
    CALC_ERR_DIV_BY_ZERO,
    CALC_ERR_OVERFLOW,
    CALC_ERR_EMPTY
} calc_status_t;

typedef struct {
    char         lineBuf[CALC_LINE_MAX_LEN];
    uint8_t      lineLen;
    calc_state_t state;
} calc_context_t;

/**
 * @brief Reset the parser state machine to CALC_STATE_IDLE.
 */
void Calc_Init(calc_context_t *ctx);

/**
 * @brief Feed one received character into the line-collection state
 *        machine. Handles backspace (0x08/0x7F) and line termination
 *        on '\r' or '\n'.
 *
 * @return true once a full line has been collected (ctx->state becomes
 *         CALC_STATE_READY); false otherwise.
 */
bool Calc_FeedChar(calc_context_t *ctx, char c);

/**
 * @brief Parse and evaluate the line currently held in ctx->lineBuf.
 *        Expected grammar: NUMBER WHITESPACE OPERATOR WHITESPACE NUMBER
 *
 * @param ctx     Context whose lineBuf holds the collected line.
 * @param result  Out-parameter: numeric result if CALC_OK is returned.
 * @return Status code describing the outcome.
 *
 * After evaluation (success or failure) the context is reset back to
 * CALC_STATE_IDLE so the next line can be collected.
 */
calc_status_t Calc_Evaluate(calc_context_t *ctx, float *result);

/**
 * @brief Human-readable string for a calc_status_t, for error reporting
 *        back over UART.
 */
const char *Calc_StatusToString(calc_status_t status);

#endif /* CALCULATOR_H */
