#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* Copias de teste das rotinas 32-bit-only usadas nos dois firmwares. */
static void fixed_to_f64_words(int32_t fixed_value, uint32_t scale,
                               uint32_t *low_word, uint32_t *high_word)
{
    const bool negative = fixed_value < 0;
    const uint32_t value = negative
        ? (uint32_t)(-(fixed_value + 1)) + 1u
        : (uint32_t)fixed_value;
    if (value == 0u) {
        *low_word = 0u;
        *high_word = negative ? 0x80000000u : 0u;
        return;
    }
    int exponent = 0;
    uint32_t numerator = value;
    uint32_t denominator = scale;
    while ((numerator >= denominator) &&
           ((numerator - denominator) >= denominator)) {
        denominator <<= 1;
        ++exponent;
    }
    while (numerator < denominator) {
        numerator <<= 1;
        --exponent;
    }
    uint32_t remainder = numerator - denominator;
    uint32_t mantissa_high = 0u;
    uint32_t mantissa_low = 0u;
    for (unsigned bit = 0u; bit < 52u; ++bit) {
        remainder <<= 1;
        mantissa_high = ((mantissa_high << 1) | (mantissa_low >> 31)) & 0xFFFFFu;
        mantissa_low <<= 1;
        if (remainder >= denominator) {
            remainder -= denominator;
            mantissa_low |= 1u;
        }
    }
    if (remainder >= (denominator - remainder)) {
        ++mantissa_low;
        if (mantissa_low == 0u) {
            ++mantissa_high;
            if (mantissa_high == 0x100000u) {
                mantissa_high = 0u;
                ++exponent;
            }
        }
    }
    *low_word = mantissa_low;
    *high_word = (negative ? 0x80000000u : 0u) |
                 ((uint32_t)(exponent + 1023) << 20) |
                 (mantissa_high & 0xFFFFFu);
}

static uint32_t divide_u32_by_60(uint32_t dividend)
{
    uint32_t quotient = 0u;
    uint32_t remainder = dividend;
    while (remainder >= 60000000u) { remainder -= 60000000u; quotient += 1000000u; }
    while (remainder >=  6000000u) { remainder -=  6000000u; quotient +=  100000u; }
    while (remainder >=   600000u) { remainder -=   600000u; quotient +=   10000u; }
    while (remainder >=    60000u) { remainder -=    60000u; quotient +=    1000u; }
    while (remainder >=     6000u) { remainder -=     6000u; quotient +=     100u; }
    while (remainder >=      600u) { remainder -=      600u; quotient +=      10u; }
    while (remainder >=       60u) { remainder -=       60u; quotient +=       1u; }
    return quotient;
}

static uint32_t pair_shift_right(uint32_t high, uint32_t low, unsigned shift)
{
    if (shift == 0u) return low;
    if (shift < 32u) return (low >> shift) | (high << (32u - shift));
    if (shift < 64u) return high >> (shift - 32u);
    return 0u;
}

static uint32_t pair_bit(uint32_t high, uint32_t low, unsigned bit)
{
    if (bit < 32u) return (low >> bit) & 1u;
    if (bit < 64u) return (high >> (bit - 32u)) & 1u;
    return 0u;
}

static void multiply_u32(uint32_t a, uint32_t b,
                         uint32_t *product_high, uint32_t *product_low)
{
    const uint32_t a0 = a & 0xFFFFu;
    const uint32_t a1 = a >> 16;
    const uint32_t b0 = b & 0xFFFFu;
    const uint32_t b1 = b >> 16;
    const uint32_t p0 = a0 * b0;
    const uint32_t p1 = a0 * b1;
    const uint32_t p2 = a1 * b0;
    const uint32_t p3 = a1 * b1;
    const uint32_t middle = (p0 >> 16) + (p1 & 0xFFFFu) + (p2 & 0xFFFFu);
    *product_low = (p0 & 0xFFFFu) | (middle << 16);
    *product_high = p3 + (p1 >> 16) + (p2 >> 16) + (middle >> 16);
}

static int32_t f64_words_to_fixed(uint32_t low_word, uint32_t high_word,
                                  uint32_t scale)
{
    const bool negative = (high_word & 0x80000000u) != 0u;
    const uint32_t exponent_bits = (high_word >> 20) & 0x7FFu;
    if (exponent_bits == 0u) return 0;
    if (exponent_bits == 0x7FFu) return negative ? -2147483647 : 2147483647;
    const int exponent = (int)exponent_bits - 1023;
    if (exponent >= 31) return negative ? -2147483647 : 2147483647;
    if (exponent < -32) return 0;

    const uint32_t significand_high = (high_word & 0xFFFFFu) | 0x100000u;
    const uint32_t significand_low = low_word;
    const unsigned shift = (unsigned)(52 - exponent);
    uint32_t integer = 0u;
    uint32_t remainder_high = significand_high;
    uint32_t remainder_low = significand_low;
    if (shift < 53u) {
        if (shift >= 32u) {
            integer = significand_high >> (shift - 32u);
            const unsigned kept_high_bits = shift - 32u;
            remainder_high = (kept_high_bits == 0u)
                ? 0u : significand_high & ((1u << kept_high_bits) - 1u);
        } else {
            integer = (significand_high << (32u - shift)) |
                      (significand_low >> shift);
            remainder_high = 0u;
            remainder_low = significand_low & ((1u << shift) - 1u);
        }
    }

    uint32_t fraction_q32 = 0u;
    bool fraction_carry = false;
    if (shift <= 32u) {
        fraction_q32 = remainder_low << (32u - shift);
    } else {
        const unsigned q_shift = shift - 32u;
        fraction_q32 = pair_shift_right(remainder_high, remainder_low, q_shift);
        if ((q_shift != 0u) && pair_bit(remainder_high, remainder_low, q_shift - 1u)) {
            ++fraction_q32;
            if (fraction_q32 == 0u) fraction_carry = true;
        }
    }
    if (fraction_carry) ++integer;

    uint32_t product_high;
    uint32_t product_low;
    multiply_u32(fraction_q32, scale, &product_high, &product_low);
    const uint32_t rounded_low = product_low + 0x80000000u;
    const uint32_t fraction_scaled = product_high + (rounded_low < product_low ? 1u : 0u);
    if ((integer > (2147483647u / scale)) ||
        ((integer * scale) > (2147483647u - fraction_scaled))) {
        return negative ? -2147483647 : 2147483647;
    }
    const uint32_t scaled = integer * scale + fraction_scaled;
    return negative ? -(int32_t)scaled : (int32_t)scaled;
}

static void check(int32_t value, uint32_t scale)
{
    union {
        double d;
        uint64_t u;
    } reference;
    uint32_t low;
    uint32_t high;
    reference.d = (double)value / (double)scale;
    fixed_to_f64_words(value, scale, &low, &high);
    assert(low == (uint32_t)reference.u);
    assert(high == (uint32_t)(reference.u >> 32));
    assert(f64_words_to_fixed(low, high, scale) == value);
}

int main(void)
{
    check(54071125, 1000000u);
    check(-1995949, 1000000u);
    check(1000, 10u);
    check(500, 1000u);
    check(1, 1000000u);
    check(-1, 1000000u);
    check(2147483000, 1000000u);
    for (int32_t value = -180000000; value <= 180000000 - 123457; value += 123457) {
        check(value, 1000000u);
    }
    for (int32_t value = -1000000; value <= 1000000 - 7919; value += 7919) {
        check(value, 10u);
    }
    assert(divide_u32_by_60(4267530u) == 71125u);
    assert(divide_u32_by_60(59756930u) == 995948u);
    assert(54 * 1000000 + (int32_t)divide_u32_by_60(4267530u) == 54071125);
    assert(-(1 * 1000000 + (int32_t)divide_u32_by_60(59756930u)) == -1995948);
    for (uint32_t value = 0u; value < 100000000u; value += 12347u) {
        assert(divide_u32_by_60(value) == value / 60u);
    }
    puts("32-bit-only IEEE-754 encode/decode: OK");
    return 0;
}
