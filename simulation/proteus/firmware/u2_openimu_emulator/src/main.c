#include <stdint.h>
#include <stdbool.h>

/*
 * Emulador de OpenIMU300ZI para o Proteus.
 *
 * Entrada:
 *   VGPS TX -> PA10 / USART1_RX, 9600 baud, NMEA 0183.
 * Saida:
 *   PA2 / USART2_TX -> U1 PA3, 115200 baud.
 *
 * A saida usa o pacote ACEINNA GPS/INS "e2" (codigo 0x6532), payload
 * oficial de 123 bytes e CRC16-CCITT (poly 0x1021, init 0x1D0F).
 */

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define RCC_BASE        0x40021000u
#define RCC_APB2ENR     REG32(RCC_BASE + 0x18u)
#define RCC_APB1ENR     REG32(RCC_BASE + 0x1Cu)

#define GPIOA_BASE      0x40010800u
#define GPIO_ODR(base)  REG32((base) + 0x0Cu)

#define USART1_BASE     0x40013800u
#define USART1_SR       REG32(USART1_BASE + 0x00u)
#define USART1_DR       REG32(USART1_BASE + 0x04u)
#define USART1_BRR      REG32(USART1_BASE + 0x08u)
#define USART1_CR1      REG32(USART1_BASE + 0x0Cu)

#define USART2_BASE     0x40004400u
#define USART2_SR       REG32(USART2_BASE + 0x00u)
#define USART2_DR       REG32(USART2_BASE + 0x04u)
#define USART2_BRR      REG32(USART2_BASE + 0x08u)
#define USART2_CR1      REG32(USART2_BASE + 0x0Cu)

#define SYST_CSR        REG32(0xE000E010u)
#define SYST_RVR        REG32(0xE000E014u)
#define SYST_CVR        REG32(0xE000E018u)
#define NVIC_ISER1      REG32(0xE000E104u)

#define RX_BUFFER_SIZE  256u

static volatile uint32_t g_ms;
static volatile uint8_t g_rx_buffer[RX_BUFFER_SIZE];
static volatile uint16_t g_rx_head;
static volatile uint16_t g_rx_tail;
static volatile uint32_t g_rx_overflow;

static bool g_gps_valid;
static uint8_t g_gps_satellites;
static int32_t g_latitude_microdegrees;
static int32_t g_longitude_microdegrees;
static int32_t g_altitude_decimeters;
static char g_latitude_hemisphere = '?';
static char g_longitude_hemisphere = '?';
static uint32_t g_last_gga_ms;

static void gpio_config(uint32_t base, unsigned pin, uint32_t nibble)
{
    volatile uint32_t *cr = (volatile uint32_t *)((pin < 8u) ? (base + 0x00u) : (base + 0x04u));
    const unsigned shift = (pin & 7u) * 4u;
    uint32_t value = *cr;
    value &= ~(0xFu << shift);
    value |= (nibble & 0xFu) << shift;
    *cr = value;
}

static void clocks_gpio_uart_init(void)
{
    /* AFIO, GPIOA e USART1 no APB2; USART2 no APB1. */
    RCC_APB2ENR |= (1u << 0) | (1u << 2) | (1u << 14);
    RCC_APB1ENR |= (1u << 17);

    /* PA10 = USART1 RX flutuante; PA2 = USART2 TX AF push-pull. */
    gpio_config(GPIOA_BASE, 10u, 0x4u);
    gpio_config(GPIOA_BASE, 2u, 0xBu);

    /* 8 MHz / 9600 = 833,33. */
    USART1_BRR = 833u;
    USART1_CR1 = (1u << 13) | (1u << 5) | (1u << 2);

    /* 8 MHz / 115200 = 69,44. */
    USART2_BRR = 69u;
    USART2_CR1 = (1u << 13) | (1u << 3);

    /* USART1 e IRQ externa 37: ISER1 bit 5. */
    NVIC_ISER1 = 1u << 5;
}

static void systick_init(void)
{
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = 7u;
}

void SysTick_Handler(void)
{
    ++g_ms;
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

void USART1_IRQHandler(void)
{
    if ((USART1_SR & (1u << 5)) != 0u) {
        rx_push((uint8_t)USART1_DR);
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

static void uart2_putc(uint8_t byte)
{
    while ((USART2_SR & (1u << 7)) == 0u) {
    }
    USART2_DR = byte;
}

static bool streq(const char *a, const char *b)
{
    while ((*a != '\0') && (*a == *b)) {
        ++a;
        ++b;
    }
    return *a == *b;
}

static uint8_t hex_value(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return (uint8_t)(c - '0');
    }
    if ((c >= 'A') && (c <= 'F')) {
        return (uint8_t)(c - 'A' + 10);
    }
    if ((c >= 'a') && (c <= 'f')) {
        return (uint8_t)(c - 'a' + 10);
    }
    return 0xFFu;
}

static int32_t parse_i32(const char *text)
{
    int32_t value = 0;
    bool negative = false;
    if (*text == '-') {
        negative = true;
        ++text;
    }
    while ((*text >= '0') && (*text <= '9')) {
        value = value * 10 + (int32_t)(*text - '0');
        ++text;
    }
    return negative ? -value : value;
}

static int32_t parse_fixed_tenths(const char *text)
{
    bool negative = false;
    int32_t value = 0;
    int32_t tenth = 0;

    if (*text == '-') {
        negative = true;
        ++text;
    }
    while ((*text >= '0') && (*text <= '9')) {
        value = value * 10 + (int32_t)(*text - '0');
        ++text;
    }
    if (*text == '.') {
        ++text;
        if ((*text >= '0') && (*text <= '9')) {
            tenth = (int32_t)(*text - '0');
        }
    }
    value = value * 10 + tenth;
    return negative ? -value : value;
}

static __attribute__((noinline)) uint32_t divide_u32_by_60(uint32_t dividend)
{
    /* Divisao por 60 somente com comparacao, subtracao e soma. */
    uint32_t quotient = 0u;
    volatile uint32_t remainder = dividend;

    while (remainder >= 60000000u) { remainder -= 60000000u; quotient += 1000000u; }
    while (remainder >=  6000000u) { remainder -=  6000000u; quotient +=  100000u; }
    while (remainder >=   600000u) { remainder -=   600000u; quotient +=   10000u; }
    while (remainder >=    60000u) { remainder -=    60000u; quotient +=    1000u; }
    while (remainder >=     6000u) { remainder -=     6000u; quotient +=     100u; }
    while (remainder >=      600u) { remainder -=      600u; quotient +=      10u; }
    while (remainder >=       60u) { remainder -=       60u; quotient +=       1u; }
    return quotient;
}

static int32_t nmea_coordinate_microdegrees(const char *text, char hemisphere)
{
    /*
     * Converte ddmm.mmmm/dddmm.mmmm primeiro em micrograus inteiros.
     * Isso evita uma conversao double->unsigned observada como defeituosa
     * no modelo VSM BLUEPILL para latitudes NMEA numericamente acima de 4096.
     */
    const unsigned degree_digits = ((hemisphere == 'N') || (hemisphere == 'S')) ? 2u : 3u;
    uint32_t degrees = 0u;
    uint32_t minute_integer = 0u;
    uint32_t minute_fraction = 0u;
    unsigned fraction_index = 0u;
    static const uint32_t fraction_places[6] = {
        100000u, 10000u, 1000u, 100u, 10u, 1u
    };

    for (unsigned i = 0u; i < degree_digits; ++i) {
        if ((text[i] < '0') || (text[i] > '9')) {
            return 0;
        }
        degrees = degrees * 10u + (uint32_t)(text[i] - '0');
    }
    text += degree_digits;

    while ((*text >= '0') && (*text <= '9')) {
        minute_integer = minute_integer * 10u + (uint32_t)(*text - '0');
        ++text;
    }
    if (*text == '.') {
        ++text;
        while ((*text >= '0') && (*text <= '9') && (fraction_index < 6u)) {
            minute_fraction += (uint32_t)(*text - '0') * fraction_places[fraction_index];
            ++fraction_index;
            ++text;
        }
    }

    const uint32_t minutes_micro = minute_integer * 1000000u + minute_fraction;
    const uint32_t minute_degrees_micro =
        divide_u32_by_60(minutes_micro + 30u);
    int32_t coordinate_micro =
        (int32_t)(degrees * 1000000u + minute_degrees_micro);
    if ((hemisphere == 'S') || (hemisphere == 'W')) {
        coordinate_micro = -coordinate_micro;
    }
    return coordinate_micro;
}

static bool valid_nmea_checksum(char *line)
{
    char *asterisk = line;
    uint8_t checksum = 0u;

    if (*line != '$') {
        return false;
    }
    while ((*asterisk != '\0') && (*asterisk != '*')) {
        ++asterisk;
    }
    if ((*asterisk != '*') || (asterisk[1] == '\0') || (asterisk[2] == '\0')) {
        return false;
    }
    for (char *p = line + 1; p < asterisk; ++p) {
        checksum ^= (uint8_t)*p;
    }
    const uint8_t hi = hex_value(asterisk[1]);
    const uint8_t lo = hex_value(asterisk[2]);
    return (hi != 0xFFu) && (lo != 0xFFu) && (checksum == (uint8_t)((hi << 4) | lo));
}

static void process_nmea_line(char *line)
{
    char *fields[20];
    unsigned count = 0u;

    if (!valid_nmea_checksum(line)) {
        return;
    }

    fields[count++] = line;
    for (char *p = line; (*p != '\0') && (count < 20u); ++p) {
        if ((*p == ',') || (*p == '*')) {
            const bool end = *p == '*';
            *p = '\0';
            if (!end) {
                fields[count++] = p + 1;
            } else {
                break;
            }
        }
    }

    if ((count >= 10u) && (streq(fields[0], "$GPGGA") || streq(fields[0], "$GNGGA"))) {
        const int32_t quality = parse_i32(fields[6]);
        g_gps_satellites = (uint8_t)parse_i32(fields[7]);
        g_gps_valid = (quality > 0) && (fields[2][0] != '\0') && (fields[4][0] != '\0');
        if (g_gps_valid) {
            g_latitude_hemisphere = fields[3][0];
            g_longitude_hemisphere = fields[5][0];
            g_latitude_microdegrees = nmea_coordinate_microdegrees(fields[2], g_latitude_hemisphere);
            g_longitude_microdegrees = nmea_coordinate_microdegrees(fields[4], g_longitude_hemisphere);
            g_altitude_decimeters = parse_fixed_tenths(fields[9]);
        }
        g_last_gga_ms = g_ms;
    }
}

static void nmea_receiver_task(void)
{
    static char line[128];
    static unsigned used;
    uint8_t byte;

    while (rx_get(&byte)) {
        if (byte == '$') {
            used = 0u;
            line[used++] = '$';
        } else if (used != 0u) {
            if ((byte == '\n') || (byte == '\r')) {
                if (used > 6u) {
                    line[used] = '\0';
                    process_nmea_line(line);
                }
                used = 0u;
            } else if (used < (sizeof line - 1u)) {
                line[used++] = (char)byte;
            } else {
                used = 0u;
            }
        }
    }
}

static uint16_t crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (unsigned bit = 0u; bit < 8u; ++bit) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static void put_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)value;
    dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16);
    dst[3] = (uint8_t)(value >> 24);
}

static void put_f32_le(uint8_t *dst, float value)
{
    union {
        float f;
        uint32_t u;
    } converter;
    converter.f = value;
    put_u32_le(dst, converter.u);
}

static void put_f64_words_le(uint8_t *dst, uint32_t low, uint32_t high)
{
    put_u32_le(dst, low);
    put_u32_le(dst + 4, high);
}

static void fixed_to_f64_words(int32_t fixed_value, uint32_t scale,
                               uint32_t *low_word, uint32_t *high_word)
{
    /*
     * Codifica fixed_value/scale em IEEE-754 binary64 usando somente uint32_t.
     * O modelo VSM BLUEPILL usado no Proteus apresentou resultados incorretos
     * em algumas rotinas auxiliares de inteiros de 64 bits.
     */
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

    /* Equivale a numerator >= 2*denominator sem arriscar overflow. */
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
    uint32_t mantissa_high = 0u; /* 20 bits mais significativos. */
    uint32_t mantissa_low = 0u;  /* 32 bits menos significativos. */

    for (unsigned bit = 0u; bit < 52u; ++bit) {
        remainder <<= 1;
        mantissa_high = ((mantissa_high << 1) | (mantissa_low >> 31)) & 0xFFFFFu;
        mantissa_low <<= 1;
        if (remainder >= denominator) {
            remainder -= denominator;
            mantissa_low |= 1u;
        }
    }

    /* Arredondamento pelo primeiro bit descartado, ainda somente em 32 bits. */
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

static void put_fixed_f64_le(uint8_t *dst, int32_t fixed_value, uint32_t scale)
{
    uint32_t low_word;
    uint32_t high_word;
    fixed_to_f64_words(fixed_value, scale, &low_word, &high_word);
    put_f64_words_le(dst, low_word, high_word);
}

static void send_compat_position_packet(void)
{
    /*
     * Pacote auxiliar exclusivo da simulacao. O e2 oficial continua sendo
     * enviado sem modificacoes; este pacote evita as limitacoes aritmeticas
     * do modelo VSM ao transportar a posicao tambem em inteiros de 32 bits.
     */
    uint8_t payload[17];
    uint16_t crc = 0x1D0Fu;
    const bool gps_fix = g_gps_valid && ((uint32_t)(g_ms - g_last_gga_ms) < 3000u);
    const uint8_t code_hi = (uint8_t)'c';
    const uint8_t code_lo = (uint8_t)'6';
    const uint8_t length = (uint8_t)sizeof payload;

    payload[0] = 2u;
    payload[1] = 6u;
    payload[2] = gps_fix ? 1u : 0u;
    put_u32_le(payload + 3, gps_fix ? (uint32_t)g_latitude_microdegrees : 0u);
    put_u32_le(payload + 7, gps_fix ? (uint32_t)g_longitude_microdegrees : 0u);
    put_u32_le(payload + 11, gps_fix ? (uint32_t)g_altitude_decimeters : 0u);
    payload[15] = (uint8_t)g_latitude_hemisphere;
    payload[16] = (uint8_t)g_longitude_hemisphere;

    uart2_putc(0x55u);
    uart2_putc(0x55u);
    uart2_putc(code_hi);
    crc = crc16_update(crc, code_hi);
    uart2_putc(code_lo);
    crc = crc16_update(crc, code_lo);
    uart2_putc(length);
    crc = crc16_update(crc, length);
    for (unsigned i = 0u; i < sizeof payload; ++i) {
        uart2_putc(payload[i]);
        crc = crc16_update(crc, payload[i]);
    }
    uart2_putc((uint8_t)(crc >> 8));
    uart2_putc((uint8_t)crc);
}

static void send_e2_packet(void)
{
    static uint32_t wave_phase;
    static uint32_t yaw_degrees;
    uint8_t payload[123];
    uint16_t crc = 0x1D0Fu;
    const bool gps_fix = g_gps_valid && ((uint32_t)(g_ms - g_last_gga_ms) < 3000u);
    const float wave = (float)((int32_t)wave_phase - 20) * 0.10f;

    for (unsigned i = 0u; i < sizeof payload; ++i) {
        payload[i] = 0u;
    }

    put_u32_le(payload + 0, g_ms);
    put_fixed_f64_le(payload + 4, (int32_t)g_ms, 1000u);
    put_f32_le(payload + 12, wave);                 /* roll, deg */
    put_f32_le(payload + 16, -0.5f * wave);         /* pitch, deg */
    put_f32_le(payload + 20, (float)yaw_degrees);   /* yaw, deg */
    put_f32_le(payload + 24, 0.010f);               /* accel X, g */
    put_f32_le(payload + 28, -0.020f);              /* accel Y, g */
    put_f32_le(payload + 32, 1.000f);               /* accel Z, g */
    put_f32_le(payload + 48, 0.10f);                /* gyro X, dps */
    put_f32_le(payload + 52, -0.20f);               /* gyro Y, dps */
    put_f32_le(payload + 56, 2.00f);                /* gyro Z, dps */
    put_f32_le(payload + 72, gps_fix ? 3.0f : 0.0f);/* vel N, m/s */
    put_f32_le(payload + 76, gps_fix ? 0.5f : 0.0f);/* vel E, m/s */
    put_f32_le(payload + 80, 0.0f);                 /* vel D, m/s */
    put_f32_le(payload + 84, 0.25f);                /* mag X, G */
    put_f32_le(payload + 88, 0.02f);                /* mag Y, G */
    put_f32_le(payload + 92, 0.42f);                /* mag Z, G */
    put_fixed_f64_le(payload + 96, gps_fix ? g_latitude_microdegrees : 0, 1000000u);
    put_fixed_f64_le(payload + 104, gps_fix ? g_longitude_microdegrees : 0, 1000000u);
    put_fixed_f64_le(payload + 112, gps_fix ? g_altitude_decimeters : 0, 10u);
    payload[120] = gps_fix ? 4u : 2u;               /* INS ou VG/AHRS */
    payload[121] = 1u;
    payload[122] = 1u;

    uart2_putc(0x55u);
    uart2_putc(0x55u);

    const uint8_t code_hi = (uint8_t)'e';
    const uint8_t code_lo = (uint8_t)'2';
    const uint8_t length = (uint8_t)sizeof payload;

    uart2_putc(code_hi);
    crc = crc16_update(crc, code_hi);
    uart2_putc(code_lo);
    crc = crc16_update(crc, code_lo);
    uart2_putc(length);
    crc = crc16_update(crc, length);

    for (unsigned i = 0u; i < sizeof payload; ++i) {
        uart2_putc(payload[i]);
        crc = crc16_update(crc, payload[i]);
    }

    /* O protocolo ACEINNA envia o CRC em ordem MSB, LSB. */
    uart2_putc((uint8_t)(crc >> 8));
    uart2_putc((uint8_t)crc);
    ++wave_phase;
    if (wave_phase >= 40u) {
        wave_phase = 0u;
    }
    ++yaw_degrees;
    if (yaw_degrees >= 360u) {
        yaw_degrees = 0u;
    }
}

int main(void)
{
    uint32_t last_packet = 0u;
    clocks_gpio_uart_init();
    systick_init();

    for (;;) {
        nmea_receiver_task();

        if ((uint32_t)(g_ms - last_packet) >= 500u) {
            last_packet += 500u;
            send_e2_packet();
            send_compat_position_packet();
        }

        /* Fallback de leitura caso um modelo VSM nao entregue a IRQ RXNE. */
        if ((USART1_SR & (1u << 5)) != 0u) {
            rx_push((uint8_t)USART1_DR);
        }
    }
}
