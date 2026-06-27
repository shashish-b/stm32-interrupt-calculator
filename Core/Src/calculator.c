/**
 * @file    calculator.c
 * @brief   Implementation of the line-collection state machine and
 *          the arithmetic expression evaluator.
 */

#include "calculator.h"
#include <stddef.h>

/* ---------------------------------------------------------------------
 * Local helpers
 * ------------------------------------------------------------------- */
static bool isDigitChar(char c)
{
    return (c >= '0' && c <= '9');
}

static bool isSpaceChar(char c)
{
    return (c == ' ' || c == '\t');
}

static void skipSpaces(const char *s, uint8_t *idx, uint8_t len)
{
    while (*idx < len && isSpaceChar(s[*idx])) {
        (*idx)++;
    }
}

/**
 * @brief Parse a (possibly signed, possibly decimal) float starting at
 *        s[*idx]. Advances *idx past the consumed characters.
 * @return true if at least one digit was consumed.
 */
static bool parseFloat(const char *s, uint8_t *idx, uint8_t len, float *out)
{
    uint8_t start = *idx;
    float sign = 1.0f;

    if (*idx < len && (s[*idx] == '+' || s[*idx] == '-')) {
        if (s[*idx] == '-') {
            sign = -1.0f;
        }
        (*idx)++;
    }

    float intPart = 0.0f;
    bool sawDigit = false;

    while (*idx < len && isDigitChar(s[*idx])) {
        intPart = (intPart * 10.0f) + (float)(s[*idx] - '0');
        (*idx)++;
        sawDigit = true;
    }

    float fracPart = 0.0f;
    float fracScale = 1.0f;

    if (*idx < len && s[*idx] == '.') {
        (*idx)++;
        while (*idx < len && isDigitChar(s[*idx])) {
            fracScale *= 10.0f;
            fracPart += (float)(s[*idx] - '0') / fracScale;
            (*idx)++;
            sawDigit = true;
        }
    }

    if (!sawDigit) {
        *idx = start; /* nothing consumed: not a number */
        return false;
    }

    *out = sign * (intPart + fracPart);
    return true;
}

/* ---------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------- */
void Calc_Init(calc_context_t *ctx)
{
    ctx->lineLen = 0;
    ctx->state = CALC_STATE_IDLE;
    ctx->lineBuf[0] = '\0';
}

bool Calc_FeedChar(calc_context_t *ctx, char c)
{
    /* Line termination: CR or LF closes out the current line,
     * regardless of whether we were IDLE or COLLECTING. Empty
     * lines (stray CR/LF) are ignored rather than evaluated. */
    if (c == '\r' || c == '\n') {
        if (ctx->lineLen == 0) {
            return false; /* ignore blank line */
        }
        ctx->lineBuf[ctx->lineLen] = '\0';
        ctx->state = CALC_STATE_READY;
        return true;
    }

    /* Backspace / DEL: erase last collected character */
    if (c == 0x08 || c == 0x7F) {
        if (ctx->lineLen > 0) {
            ctx->lineLen--;
        }
        return false;
    }

    /* Normal character: append if there's room */
    if (ctx->lineLen < (CALC_LINE_MAX_LEN - 1u)) {
        ctx->lineBuf[ctx->lineLen++] = c;
        ctx->state = CALC_STATE_COLLECTING;
    }
    /* Silently drop characters beyond CALC_LINE_MAX_LEN-1; the eventual
     * '\0'-terminated buffer is still well-formed for evaluation. */

    return false;
}

calc_status_t Calc_Evaluate(calc_context_t *ctx, float *result)
{
    uint8_t idx = 0;
    uint8_t len = ctx->lineLen;
    calc_status_t status;

    skipSpaces(ctx->lineBuf, &idx, len);

    if (idx >= len) {
        status = CALC_ERR_EMPTY;
        goto done;
    }

    float lhs;
    if (!parseFloat(ctx->lineBuf, &idx, len, &lhs)) {
        status = CALC_ERR_SYNTAX;
        goto done;
    }

    skipSpaces(ctx->lineBuf, &idx, len);

    if (idx >= len) {
        status = CALC_ERR_SYNTAX;
        goto done;
    }

    char op = ctx->lineBuf[idx];
    if (op != '+' && op != '-' && op != '*' && op != '/') {
        status = CALC_ERR_SYNTAX;
        goto done;
    }
    idx++;

    skipSpaces(ctx->lineBuf, &idx, len);

    float rhs;
    if (!parseFloat(ctx->lineBuf, &idx, len, &rhs)) {
        status = CALC_ERR_SYNTAX;
        goto done;
    }

    skipSpaces(ctx->lineBuf, &idx, len);

    if (idx != len) {
        /* trailing garbage after a seemingly valid expression */
        status = CALC_ERR_SYNTAX;
        goto done;
    }

    switch (op) {
        case '+':
            *result = lhs + rhs;
            break;
        case '-':
            *result = lhs - rhs;
            break;
        case '*':
            *result = lhs * rhs;
            break;
        case '/':
            if (rhs == 0.0f) {
                status = CALC_ERR_DIV_BY_ZERO;
                goto done;
            }
            *result = lhs / rhs;
            break;
        default:
            status = CALC_ERR_SYNTAX; /* unreachable, kept for safety */
            goto done;
    }

    status = CALC_OK;

done:
    /* Always reset for the next line, success or failure */
    Calc_Init(ctx);
    return status;
}

const char *Calc_StatusToString(calc_status_t status)
{
    switch (status) {
        case CALC_OK:               return "OK";
        case CALC_ERR_SYNTAX:       return "ERR: syntax (expected: <num> <op> <num>)";
        case CALC_ERR_DIV_BY_ZERO:  return "ERR: division by zero";
        case CALC_ERR_OVERFLOW:     return "ERR: overflow";
        case CALC_ERR_EMPTY:        return "ERR: empty input";
        default:                    return "ERR: unknown";
    }
}
