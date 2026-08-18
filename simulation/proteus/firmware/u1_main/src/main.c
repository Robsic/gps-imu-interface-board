#include <stdint.h>
#include <stdbool.h>

/*
 * Receptor principal U1 para validacao integrada no Proteus.
 *
 * - Mantem os dois encoders A/B/Z da versao 1.3.
 * - Recebe o pacote oficial ACEINNA GPS/INS "e2" em PA3/USART2_RX.
 * - Mostra diagnostico em PA2/USART2_TX, 115200 baud.
 * - Valida preambulo, codigo, tamanho e CRC16-CCITT.
 *
 * O CAN continua desabilitado porque o modelo BLUEPILL VSM de terceiros
 * encerra/trava a simulacao ao acessar os registradores CAN.
 */

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     REG32(RCC_BASE + 0x18u)
#define RCC_APB1ENR     REG32(RCC_BASE + 0x1Cu)

#define AFIO_BASE       0x40010000u
#define AFIO_EXTICR3    REG32(AFIO_BASE + 0x10u)

#define EXTI_BASE       0x40010400u
#define EXTI_IMR        REG32(EXTI_BASE + 0x00u)
#define EXTI_RTSR       REG32(EXTI_BASE + 0x08u)
#define EXTI_PR         REG32(EXTI_BASE + 0x14u)

#define GPIOA_BASE      0x40010800u
#define GPIOB_BASE      0x40010C00u
#define GPIO_IDR(base)  REG32((base) + 0x08u)
#define GPIO_ODR(base)  REG32((base) + 0x0Cu)

#define USART2_BASE     0x40004400u
#define USART2_SR       REG32(USART2_BASE + 0x00u)
#define USART2_DR       REG32(USART2_BASE + 0x04u)
#define USART2_BRR      REG32(USART2_BASE + 0x08u)
#define USART2_CR1      REG32(USART2_BASE + 0x0Cu)

#define SYST_CSR        REG32(0xE000E010u)
#define SYST_RVR        REG32(0xE000E014u)
#define SYST_CVR        REG32(0xE000E018u)
#define NVIC_ISER0      REG32(0xE000E100u)
#define NVIC_ISER1      REG32(0xE000E104u)

#define RX_BUFFER_SIZE  256u

typedef struct {
    uint32_t timer_ms;
    float roll;
    float pitch;
    float yaw;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
    float vel_n;
    float vel_e;
    float vel_d;
    int32_t latitude_microdegrees;
    int32_t longitude_microdegrees;
    int32_t altitude_decimeters;
    uint8_t mode;
    uint8_t linear_accel_switch;
    uint8_t turn_switch;
} e2_data_t;

static volatile uint32_t g_ms;
static volatile uint32_t g_z_left;
static volatile uint32_t g_z_right;
static int32_t g_encoder_left;
static int32_t g_encoder_right;
static uint8_t g_prev_left;
static uint8_t g_prev_right;
static bool g_encoder_sample_ready;

static volatile uint8_t g_rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t g_rx_head;
static volatile uint16_t g_rx_tail;
static volatile uint32_t g_rx_overflow;

static e2_data_t g_e2;
static volatile bool g_new_e2;
static uint32_t g_e2_packets;
static uint32_t g_crc_errors;
static uint32_t g_last_e2_ms;
static bool g_compat_seen;
static bool g_compat_fix;
static uint8_t g_compat_major;
static uint8_t g_compat_minor;
static int32_t g_compat_latitude_microdegrees;
static int32_t g_compat_longitude_microdegrees;
static int32_t g_compat_altitude_decimeters;
static char g_compat_latitude_hemisphere;
static char g_compat_longitude_hemisphere;

static void gpio_config(uint32_t base, unsigned pin, uint32_t nibble)
{
    volatile uint32_t *cr = (volatile uint32_t *)((pin < 8u) ? (base + 0x00u) : (base + 0x04u));
    const unsigned shift = (pin & 7u) * 4u;
    uint32_t value = *cr;
    value &= ~(0xFu << shift);
    value |= (nibble & 0xFu) << shift;
    *cr = value;
}

static void clocks_and_gpio_init(void)
{
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 3);
    RCC_APB1ENR |= (1u << 17);

    /* Entradas dos encoders: pull-up. */
    gpio_config(GPIOA_BASE, 0u, 0x8u);
    gpio_config(GPIOA_BASE, 1u, 0x8u);
    gpio_config(GPIOB_BASE, 6u, 0x8u);
    gpio_config(GPIOB_BASE, 7u, 0x8u);
    gpio_config(GPIOB_BASE, 8u, 0x8u);
    gpio_config(GPIOB_BASE, 9u, 0x8u);
    GPIO_ODR(GPIOA_BASE) |= (1u << 0) | (1u << 1);
    GPIO_ODR(GPIOB_BASE) |= (1u << 6) | (1u << 7) | (1u << 8) | (1u << 9);

    /* USART2: PA2 TX AF push-pull; PA3 RX flutuante. */
    gpio_config(GPIOA_BASE, 2u, 0xBu);
    gpio_config(GPIOA_BASE, 3u, 0x4u);

    /* Saidas internas de estimulo para os encoders e pulsos Z. */
    for (unsigned pin = 10u; pin <= 15u; ++pin) {
        gpio_config(GPIOB_BASE, pin, 0x3u);
    }
    GPIO_ODR(GPIOB_BASE) &= ~((0x3Fu) << 10);
}

static void uart_init(void)
{
    /* 8 MHz / 115200 = 69,44. RXNE gera IRQ para nao perder pacote e2. */
    USART2_BRR = 69u;
    USART2_CR1 = (1u << 13) | (1u << 5) | (1u << 3) | (1u << 2);
    NVIC_ISER1 = 1u << 6; /* USART2 IRQ38 -> ISER1 bit 6. */
}

static void z_interrupts_init(void)
{
    AFIO_EXTICR3 = (AFIO_EXTICR3 & ~0xFFu) | 0x11u;
    EXTI_PR = (1u << 8) | (1u << 9);
    EXTI_RTSR |= (1u << 8) | (1u << 9);
    EXTI_IMR |= (1u << 8) | (1u << 9);
    NVIC_ISER0 = 1u << 23;
}

static void systick_init(void)
{
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = 7u;
}

static void uart_putc(char c)
{
    while ((USART2_SR & (1u << 7)) == 0u) {
    }
    USART2_DR = (uint32_t)(uint8_t)c;
}

static void uart_puts(const char *text)
{
    while (*text != '\0') {
        uart_putc(*text++);
    }
}

static void uart_put_u64(uint64_t value)
{
    char buffer[20];
    unsigned used = 0u;
    if (value == 0u) {
        uart_putc('0');
        return;
    }
    while ((value != 0u) && (used < sizeof buffer)) {
        buffer[used++] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    while (used != 0u) {
        uart_putc(buffer[--used]);
    }
}

static void uart_put_u32(uint32_t value)
{
    uart_put_u64(value);
}

static void uart_put_i32(int32_t value)
{
    if (value < 0) {
        uart_putc('-');
        uart_put_u32((uint32_t)(-(value + 1)) + 1u);
    } else {
        uart_put_u32((uint32_t)value);
    }
}

static void uart_put_fixed(double value, unsigned decimals)
{
    static const uint32_t scales[] = {1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u};
    const uint32_t scale = scales[(decimals <= 6u) ? decimals : 6u];
    if (value < 0.0) {
        uart_putc('-');
        value = -value;
    }
    const uint64_t scaled = (uint64_t)(value * (double)scale + 0.5);
    const uint64_t integer = scaled / scale;
    uint64_t fraction = scaled % scale;
    uart_put_u64(integer);
    if (decimals != 0u) {
        uart_putc('.');
        uint32_t divisor = scale / 10u;
        while (divisor != 0u) {
            uart_putc((char)('0' + (fraction / divisor) % 10u));
            divisor /= 10u;
        }
    }
}

static void uart_put_fixed_i32(int32_t scaled_value, unsigned decimals)
{
    static const uint32_t scales[] = {1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u};
    const uint32_t scale = scales[(decimals <= 6u) ? decimals : 6u];
    uint32_t absolute;
    if (scaled_value < 0) {
        uart_putc('-');
        absolute = (uint32_t)(-(scaled_value + 1)) + 1u;
    } else {
        absolute = (uint32_t)scaled_value;
    }
    uart_put_u32(absolute / scale);
    if (decimals != 0u) {
        uart_putc('.');
        const uint32_t fraction = absolute % scale;
        uint32_t divisor = scale / 10u;
        while (divisor != 0u) {
            uart_putc((char)('0' + (fraction / divisor) % 10u));
            divisor /= 10u;
        }
    }
}

static void rx_push(uint8_t byte)
{
    const uint16_t next = (uint16_t)((g_rx_head + 1u) & (RX_BUFFER_SIZE - 1u));
    if (next != g_rx_tail) {
        g_rx_buffer[g_rx_head] = byte;
        g_rx_head = next;
    } else {
        ++g_rx_overflow;
    }
}

void USART2_IRQHandler(void)
{
    if ((USART2_SR & (1u << 5)) != 0u) {
        rx_push((uint8_t)USART2_DR);
    }
}

static bool rx_get(uint8_t *byte)
{
    if (g_rx_tail == g_rx_head) {
        return false;
    }
    *byte = g_rx_buffer[g_rx_tail];
    g_rx_tail = (uint16_t)((g_rx_tail + 1u) & (RX_BUFFER_SIZE - 1u));
    return true;
}

static uint16_t crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (unsigned bit = 0u; bit < 8u; ++bit) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint32_t read_u32_le(const uint8_t *src)
{
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static float read_f32_le(const uint8_t *src)
{
    union {
        uint32_t u;
        float f;
    } converter;
    converter.u = read_u32_le(src);
    return converter.f;
}

static uint32_t pair_shift_right(uint32_t high, uint32_t low, unsigned shift)
{
    if (shift == 0u) {
        return low;
    }
    if (shift < 32u) {
        return (low >> shift) | (high << (32u - shift));
    }
    if (shift < 64u) {
        return high >> (shift - 32u);
    }
    return 0u;
}

static uint32_t pair_bit(uint32_t high, uint32_t low, unsigned bit)
{
    if (bit < 32u) {
        return (low >> bit) & 1u;
    }
    if (bit < 64u) {
        return (high >> (bit - 32u)) & 1u;
    }
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

static int32_t read_f64_as_fixed_i32(const uint8_t *src, uint32_t scale)
{
    /*
     * Decodifica IEEE-754 binary64 para inteiro escalado usando apenas
     * uint32_t. Isso contorna as rotinas de 64 bits defeituosas do VSM.
     */
    const uint32_t low_word = read_u32_le(src);
    const uint32_t high_word = read_u32_le(src + 4);
    const bool negative = (high_word & 0x80000000u) != 0u;
    const uint32_t exponent_bits = (high_word >> 20) & 0x7FFu;

    if (exponent_bits == 0u) {
        return 0;
    }
    if (exponent_bits == 0x7FFu) {
        return negative ? -2147483647 : 2147483647;
    }

    const int exponent = (int)exponent_bits - 1023;
    if (exponent >= 31) {
        return negative ? -2147483647 : 2147483647;
    }
    if (exponent < -32) {
        return 0;
    }

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
                ? 0u
                : significand_high & ((1u << kept_high_bits) - 1u);
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
            if (fraction_q32 == 0u) {
                fraction_carry = true;
            }
        }
    }

    if (fraction_carry) {
        ++integer;
    }

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

static void decode_e2(const uint8_t *payload)
{
    g_e2.timer_ms = read_u32_le(payload + 0);
    g_e2.roll = read_f32_le(payload + 12);
    g_e2.pitch = read_f32_le(payload + 16);
    g_e2.yaw = read_f32_le(payload + 20);
    g_e2.accel_x = read_f32_le(payload + 24);
    g_e2.accel_y = read_f32_le(payload + 28);
    g_e2.accel_z = read_f32_le(payload + 32);
    g_e2.gyro_x = read_f32_le(payload + 48);
    g_e2.gyro_y = read_f32_le(payload + 52);
    g_e2.gyro_z = read_f32_le(payload + 56);
    g_e2.vel_n = read_f32_le(payload + 72);
    g_e2.vel_e = read_f32_le(payload + 76);
    g_e2.vel_d = read_f32_le(payload + 80);
    g_e2.latitude_microdegrees = read_f64_as_fixed_i32(payload + 96, 1000000u);
    g_e2.longitude_microdegrees = read_f64_as_fixed_i32(payload + 104, 1000000u);
    g_e2.altitude_decimeters = read_f64_as_fixed_i32(payload + 112, 10u);
    g_e2.mode = payload[120];
    g_e2.linear_accel_switch = payload[121];
    g_e2.turn_switch = payload[122];
    ++g_e2_packets;
    g_last_e2_ms = g_ms;
    g_new_e2 = true;
}

static void decode_compat_position(const uint8_t *payload)
{
    g_compat_major = payload[0];
    g_compat_minor = payload[1];
    g_compat_fix = payload[2] != 0u;
    g_compat_latitude_microdegrees = (int32_t)read_u32_le(payload + 3);
    g_compat_longitude_microdegrees = (int32_t)read_u32_le(payload + 7);
    g_compat_altitude_decimeters = (int32_t)read_u32_le(payload + 11);
    g_compat_latitude_hemisphere = (char)payload[15];
    g_compat_longitude_hemisphere = (char)payload[16];
    g_compat_seen = true;
}

static void aceinna_parser_task(void)
{
    enum {
        WAIT_55_1,
        WAIT_55_2,
        READ_CODE_HI,
        READ_CODE_LO,
        READ_LENGTH,
        READ_PAYLOAD,
        READ_CRC_HI,
        READ_CRC_LO
    };
    static unsigned state;
    static uint16_t code;
    static uint8_t length;
    static uint8_t payload[192];
    static unsigned payload_index;
    static uint16_t crc;
    static uint16_t received_crc;
    uint8_t byte;

    while (rx_get(&byte)) {
        switch (state) {
        case WAIT_55_1:
            if (byte == 0x55u) {
                state = WAIT_55_2;
            }
            break;
        case WAIT_55_2:
            state = (byte == 0x55u) ? READ_CODE_HI : WAIT_55_1;
            break;
        case READ_CODE_HI:
            code = (uint16_t)byte << 8;
            crc = crc16_update(0x1D0Fu, byte);
            state = READ_CODE_LO;
            break;
        case READ_CODE_LO:
            code |= byte;
            crc = crc16_update(crc, byte);
            state = READ_LENGTH;
            break;
        case READ_LENGTH:
            length = byte;
            crc = crc16_update(crc, byte);
            payload_index = 0u;
            state = (length == 0u) ? READ_CRC_HI : ((length <= sizeof payload) ? READ_PAYLOAD : WAIT_55_1);
            break;
        case READ_PAYLOAD:
            payload[payload_index++] = byte;
            crc = crc16_update(crc, byte);
            if (payload_index >= length) {
                state = READ_CRC_HI;
            }
            break;
        case READ_CRC_HI:
            received_crc = (uint16_t)byte << 8;
            state = READ_CRC_LO;
            break;
        case READ_CRC_LO:
            received_crc |= byte;
            if (received_crc == crc) {
                if ((code == 0x6532u) && (length == 123u)) {
                    decode_e2(payload);
                } else if ((code == 0x6336u) && (length == 17u)) {
                    decode_compat_position(payload);
                }
            } else {
                ++g_crc_errors;
            }
            state = WAIT_55_1;
            break;
        default:
            state = WAIT_55_1;
            break;
        }
    }
}

static void stimulus_step(void)
{
    static const uint8_t forward[4] = {0u, 1u, 3u, 2u};
    static const uint8_t reverse[4] = {0u, 2u, 3u, 1u};
    static uint32_t step;
    uint32_t odr = GPIO_ODR(GPIOB_BASE);

    odr &= ~((0x3Fu) << 10);
    odr |= ((uint32_t)forward[step & 3u]) << 10;
    odr |= ((uint32_t)reverse[step & 3u]) << 12;
    if ((step % 128u) == 0u) {
        odr |= 1u << 14;
    }
    if ((step % 192u) == 0u) {
        odr |= 1u << 15;
    }
    GPIO_ODR(GPIOB_BASE) = odr;
    ++step;
}

static void encoder_software_sample(void)
{
    static const int32_t delta[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0
    };
    const uint32_t gpioa = GPIO_IDR(GPIOA_BASE);
    const uint32_t gpiob = GPIO_IDR(GPIOB_BASE);
    const uint8_t left = (uint8_t)((((gpioa >> 0) & 1u) << 1) | ((gpioa >> 1) & 1u));
    const uint8_t right = (uint8_t)((((gpiob >> 6) & 1u) << 1) | ((gpiob >> 7) & 1u));

    if (!g_encoder_sample_ready) {
        g_prev_left = left;
        g_prev_right = right;
        g_encoder_sample_ready = true;
        return;
    }
    g_encoder_left += delta[(g_prev_left << 2) | left];
    g_encoder_right += delta[(g_prev_right << 2) | right];
    g_prev_left = left;
    g_prev_right = right;
}

void SysTick_Handler(void)
{
    ++g_ms;
}

void EXTI9_5_IRQHandler(void)
{
    const uint32_t pending = EXTI_PR;
    if ((pending & (1u << 8)) != 0u) {
        ++g_z_left;
        EXTI_PR = 1u << 8;
    }
    if ((pending & (1u << 9)) != 0u) {
        ++g_z_right;
        EXTI_PR = 1u << 9;
    }
}

static void report_e2(void)
{
    const bool use_compat = g_compat_seen &&
                            (g_compat_major == 2u) &&
                            (g_compat_minor >= 5u);
    const int32_t latitude = use_compat
        ? g_compat_latitude_microdegrees : g_e2.latitude_microdegrees;
    const int32_t longitude = use_compat
        ? g_compat_longitude_microdegrees : g_e2.longitude_microdegrees;
    const int32_t altitude = use_compat
        ? g_compat_altitude_decimeters : g_e2.altitude_decimeters;

    uart_puts("E2=OK CRC=OK PKT=");
    uart_put_u32(g_e2_packets);
    uart_puts(" POS=");
    uart_puts(use_compat ? "COMPAT32" : "E2");
    uart_puts(" EMU=");
    if (g_compat_seen) {
        uart_put_u32(g_compat_major);
        uart_putc('.');
        uart_put_u32(g_compat_minor);
    } else {
        uart_puts("NA");
    }
    uart_puts(" HEM=");
    if (g_compat_seen) {
        uart_putc(g_compat_latitude_hemisphere);
        uart_putc(g_compat_longitude_hemisphere);
    } else {
        uart_puts("NA");
    }
    uart_puts(" MODE=");
    uart_puts((g_e2.mode == 4u) ? "INS" : "VG");
    uart_puts(" GPS=");
    uart_puts(use_compat ? (g_compat_fix ? "FIX" : "NOFIX")
                         : ((g_e2.mode == 4u) ? "FIX" : "NOFIX"));
    uart_puts(" LAT=");
    uart_put_fixed_i32(latitude, 6u);
    uart_puts(" LON=");
    uart_put_fixed_i32(longitude, 6u);
    uart_puts(" ALT=");
    uart_put_fixed_i32(altitude, 1u);
    uart_puts(" RPY=");
    uart_put_fixed(g_e2.roll, 2u);
    uart_putc(',');
    uart_put_fixed(g_e2.pitch, 2u);
    uart_putc(',');
    uart_put_fixed(g_e2.yaw, 2u);
    uart_puts(" ACC=");
    uart_put_fixed(g_e2.accel_x, 3u);
    uart_putc(',');
    uart_put_fixed(g_e2.accel_y, 3u);
    uart_putc(',');
    uart_put_fixed(g_e2.accel_z, 3u);
    uart_puts(" L=");
    uart_put_i32(g_encoder_left);
    uart_puts(" R=");
    uart_put_i32(g_encoder_right);
    uart_puts(" ZL=");
    uart_put_u32(g_z_left);
    uart_puts(" ZR=");
    uart_put_u32(g_z_right);
    uart_puts(" CAN=NA(PROTEUS)\r\n");
}

int main(void)
{
    uint32_t last_stimulus = 0u;
    uint32_t last_wait_report = 0u;
    clocks_and_gpio_init();
    uart_init();
    z_interrupts_init();
    systick_init();

    uart_puts("\r\nIMU_GPS_VALIDATION v2.6 ACEINNA E2\r\n");
    uart_puts("CLK=8MHz UART2=115200 ENC=SW CAN=NA(PROTEUS)\r\n");
    uart_puts("Aguardando U2/OpenIMU em PA3 e pacotes 0x5555 0x6532 LEN=123.\r\n");

    for (;;) {
        const uint32_t now = g_ms;

        if ((uint32_t)(now - last_stimulus) >= 2u) {
            last_stimulus += 2u;
            encoder_software_sample();
            stimulus_step();
        }

        aceinna_parser_task();

        if (g_new_e2) {
            g_new_e2 = false;
            report_e2();
        }

        if (((uint32_t)(now - g_last_e2_ms) >= 1500u) &&
            ((uint32_t)(now - last_wait_report) >= 1000u)) {
            last_wait_report = now;
            uart_puts("E2=WAIT CRC_ERR=");
            uart_put_u32(g_crc_errors);
            uart_puts(" RX_OVF=");
            uart_put_u32(g_rx_overflow);
            uart_puts(" Verifique U2.PA2 -> U1.PA3 e 115200 baud.\r\n");
        }

        /* Fallback caso o modelo VSM nao entregue a IRQ RXNE. */
        if ((USART2_SR & (1u << 5)) != 0u) {
            rx_push((uint8_t)USART2_DR);
        }
    }
}
