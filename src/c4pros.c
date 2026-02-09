#include <stdint.h>
#include "c4pros.h"

inline int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

inline void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void c4pros_get_string(char *buf, uint16_t max) {
    static char echo[2];
    uint16_t i = 0;
    uint16_t pos = c4pros_cursor_get();
    uint8_t row = (uint8_t)(pos >> 8);
    uint8_t col = (uint8_t)(pos & 0xFF);
    if (max == 0) return;
    for (;;) {
        char c = (char)c4pros_get_char();
        if (c == '\n' || c == '\r') {
            c4pros_print_newline();
            break;
        }
        if (c == '\b') {   /* Backspace: стираем через вывод "\b \b", чтобы курсор PRos (INT 0x21) не расходился с нашим (INT 10h). */
            if (i > 0) {
                i--;
                buf[i] = '\0';
                if (col > 0)
                    col--;
                else if (row > 0) {
                    row--;
                    col = 79;
                }
                {
                    static const char bs_space_bs[4] = { '\b', ' ', '\b', '\0' };
                    c4pros_print_white(bs_space_bs);
                }
            }
            continue;
        }
        if (i + 1 < max) {
            buf[i++] = c;
            buf[i] = '\0';
        }
        echo[0] = c;
        echo[1] = '\0';
        c4pros_print_white(echo);
        col++;
        if (col > 79) {
            col = 0;
            row++;
        }
    }
    buf[i] = '\0';
}

/* Пиксель через BIOS INT 10h AH=0x0C */
inline void c4pros_set_pixel(uint8_t color, uint16_t x, uint16_t y) {
    __asm__ volatile (
        "movb $0x0C, %%ah\n\t"
        "movb %0, %%al\n\t"
        "xorw %%bx, %%bx\n\t"
        "movw %1, %%cx\n\t"
        "movw %2, %%dx\n\t"
        "int $0x10"
        :
        : "g" ((uint8_t)color), "g" ((uint16_t)x), "g" ((uint16_t)y)
        : "ax", "bx", "cx", "dx", "cc", "memory"
    );
}

/* Пиксель записью в 0xA000 (режим 0x12 planar). Без восстановления GC — быстрее. */
void c4pros_mem_pixel(uint8_t color, uint16_t x, uint16_t y) {
    uint16_t offset = (uint16_t)(y * 80u + (x / 8u));
    uint8_t bit_mask = (uint8_t)(1u << (7 - (x & 7u)));

    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %2, %%di\n\t"
        "movw $0x3CE, %%dx\n\t"
        /* GC reg 0 = color */
        "xorb %%al, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %0, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        /* GC reg 1 = 0xFF */
        "movb $1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0xFF, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        /* GC reg 8 = bit_mask */
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        /* read (latch) + write */
        "movb %%es:(%%di), %%al\n\t"
        "movb $0xFF, %%es:(%%di)\n\t"
        "popw %%es"
        :
        : "g" (color), "g" (bit_mask), "g" (offset)
        : "ax", "dx", "di", "cc", "memory"
    );
}

/* Горизонтальная линия записью по байтам (режим 0x12 planar). Значительно быстрее fill по пикселям. */
void c4pros_mem_hline(uint8_t color, uint16_t x, uint16_t y, uint16_t len) {
    if (len == 0) return;
    uint16_t offset = (uint16_t)(y * 80u + (x / 8u));
    uint8_t start_shift = (uint8_t)(x & 7u);
    uint16_t first_n = 8u - start_shift;
    if (first_n > len) first_n = len;
    uint8_t first_mask = (first_n == 8u) ? 0xFFu : (uint8_t)(((1u << first_n) - 1u) << (8u - start_shift - first_n));
    uint16_t rest = len - first_n;
    uint16_t num_full = rest / 8u;
    uint16_t last_n = rest % 8u;
    uint8_t last_mask = (last_n == 0) ? 0u : (uint8_t)(((1u << last_n) - 1u) << (8u - last_n));

    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %2, %%di\n\t"
        "movw $0x3CE, %%dx\n\t"
        /* GC reg 0 = color, reg 1 = 0xFF */
        "xorb %%al, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %0, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb $1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0xFF, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        /* First partial byte */
        "movb %1, %%al\n\t"
        "testb %%al, %%al\n\t"
        "jz 1f\n\t"
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb %%es:(%%di), %%al\n\t"
        "movb $0xFF, %%es:(%%di)\n\t"
        "incw %%di\n\t"
        "1:\n\t"
        /* Full bytes */
        "movw %3, %%cx\n\t"
        "jcxz 2f\n\t"
        "3:\n\t"
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0xFF, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb %%es:(%%di), %%al\n\t"
        "movb $0xFF, %%es:(%%di)\n\t"
        "incw %%di\n\t"
        "loop 3b\n\t"
        "2:\n\t"
        /* Last partial byte */
        "movb %4, %%al\n\t"
        "testb %%al, %%al\n\t"
        "jz 4f\n\t"
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %4, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb %%es:(%%di), %%al\n\t"
        "movb $0xFF, %%es:(%%di)\n\t"
        "4:\n\t"
        "popw %%es"
        :
        : "q" (color), "m" (first_mask), "m" (offset), "m" (num_full), "m" (last_mask)
        : "ax", "cx", "dx", "di", "cc", "memory"
    );
}

/* Чтение байта из 0xA000 (режим 0x12): plane 0..3, GC reg 4 = Read Map Select. */
uint8_t c4pros_mem_read_byte(uint16_t offset, uint8_t plane)
{
    uint8_t r;
    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %1, %%di\n\t"
        "movw $0x3CE, %%dx\n\t"
        "movb $4, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %2, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movb %%es:(%%di), %%al\n\t"
        "movb %%al, %0\n\t"
        "popw %%es"
        : "=m" (r)
        : "r" (offset), "q" (plane)
        : "ax", "dx", "di", "cc", "memory"
    );
    return r;
}

/* Запись байта в одну плоскость: SC (0x3C4) reg 2 = Map Mask. plane 0..3. */
void c4pros_mem_write_byte(uint16_t offset, uint8_t plane, uint8_t byte)
{
    uint8_t mask = (uint8_t)(1u << plane);
    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %0, %%di\n\t"
        "movw $0x3C4, %%dx\n\t"
        "movb $2, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movb %2, %%al\n\t"
        "movb %%al, %%es:(%%di)\n\t"
        "popw %%es"
        :
        : "r" (offset), "q" (mask), "m" (byte)
        : "ax", "dx", "di", "cc", "memory"
    );
}

/* Восстановить Map Mask (SC 0x3C4 reg 2) = все плоскости, после операций с курсором. */
void c4pros_mem_sc_restore(void)
{
    __asm__ volatile (
        "movw $0x3C4, %%dx\n\t"
        "movb $2, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0x0F, %%al\n\t"
        "outb %%al, %%dx"
        :
        :
        : "ax", "dx", "cc"
    );
}

/* После серии out_mem_pixel вызови один раз, если нужны стандартные регистры VGA. */
void c4pros_mem_pixel_gc_restore(void) {
    __asm__ volatile (
        "movw $0x3CE, %%dx\n\t"
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0xFF, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb $1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "xorb %%al, %%al\n\t"
        "outb %%al, %%dx"
        :
        :
        : "ax", "dx", "cc"
    );
}

/* --- PS/2 Controller (порты 0x60, 0x64) --- */
#define PS2_STATUS  0x64
#define PS2_DATA    0x60
#define PS2_STAT_OBF  0x01u   /* output buffer full — можно читать из 0x60 */
#define PS2_STAT_IBF  0x02u   /* input buffer full — нельзя писать */
#define PS2_STAT_AUX  0x20u   /* байт в 0x60 от мыши */

static inline uint8_t ps2_inb(uint16_t port)
{
    uint8_t r;
    __asm__ volatile ("inb %%dx, %0" : "=a" (r) : "d" (port) : "memory");
    return r;
}
static inline void ps2_outb(uint8_t val, uint16_t port)
{
    __asm__ volatile ("outb %b0, %%dx" : : "a" (val), "d" (port) : "memory");
}

/* Ждать, пока можно писать в контроллер (бит IBF сброшен). */
static void ps2_wait_write(void)
{
    unsigned n = 0;
    while ((ps2_inb(PS2_STATUS) & PS2_STAT_IBF) && ++n < 10000u) { }
}

/* Ждать, пока в 0x60 есть байт (бит OBF установлен). Возвращает 1, если данные от мыши (AUX). */
static int ps2_wait_read_mouse(uint8_t *from_mouse)
{
    unsigned n = 0;
    for (;;) {
        uint8_t s = ps2_inb(PS2_STATUS);
        if (s & PS2_STAT_OBF) {
            if (from_mouse) *from_mouse = (s & PS2_STAT_AUX) ? 1 : 0;
            return 1;
        }
        if (++n >= 20000u) return 0;
    }
}

static uint8_t ps2_read_data(void)
{
    return ps2_inb(PS2_DATA);
}

/* Отправить команду мыши (0xD4 в 0x64, затем cmd в 0x60). Ждать ACK 0xFA. */
static int ps2_send_mouse_cmd(uint8_t cmd)
{
    ps2_wait_write();
    ps2_outb(0xD4, PS2_STATUS);
    ps2_wait_write();
    ps2_outb(cmd, PS2_DATA);
    for (unsigned i = 0; i < 50; i++) {
        uint8_t from_mouse;
        if (!ps2_wait_read_mouse(&from_mouse)) return 0;
        uint8_t b = ps2_read_data();
        if (from_mouse && b == 0xFA) return 1;
    }
    return 0;
}

/* Состояние приёма 3-байтового пакета мыши. */
static uint8_t ps2_mouse_packet_buf[3];
static uint8_t ps2_mouse_packet_idx;
static int ps2_mouse_inited;

/* Инициализация PS/2 мыши: включить aux, включить поток пакетов. Возврат: 0 = ок, -1 = ошибка. */
int c4pros_ps2_mouse_init(void)
{
    ps2_mouse_packet_idx = 0;
    ps2_mouse_inited = 0;

    /* Включить auxiliary device */
    ps2_wait_write();
    ps2_outb(0xA8, PS2_STATUS);
    ps2_wait_write();

    /* Сброс к умолчаниям (опционально, уменьшает мусор после включения) */
    if (!ps2_send_mouse_cmd(0xF6)) return -1;

    /* Включить streaming (мышь шлёт пакеты при движении/нажатии) */
    if (!ps2_send_mouse_cmd(0xF4)) return -1;

    /* Очистить буфер 0x60 от остатков (ACK 0xFA и т.д.), иначе первый байт пакета теряется и poll не завершает пакет */
    while (ps2_inb(PS2_STATUS) & PS2_STAT_OBF)
        (void)ps2_read_data();

    ps2_mouse_packet_idx = 0;
    ps2_mouse_inited = 1;
    return 0;
}

/* Есть ли байт в буфере 0x60? Читаем и мышь, и клавиатуру; пакет мыши выделяем по биту 3 первого байта. */
int c4pros_ps2_mouse_byte_ready(void)
{
    uint8_t s = ps2_inb(PS2_STATUS);
    /* В QEMU и на части железа бит AUX не выставляется — смотрим только OBF */
    return (s & PS2_STAT_OBF) ? 1 : 0;
}

/* Прочитать один пакет мыши (3 байта), если готов. Возврат: 1 = пакет прочитан, 0 = нет данных/неполный пакет.
   Синхронизация: первый байт пакета мыши в формате Microsoft всегда имеет бит 3 = 1; иначе считаем байт клавиатуры и пропускаем.
   *buttons: биты 0=левый, 1=правый, 2=средний.
   *dx, *dy: относительное перемещение (со знаком). */
int c4pros_ps2_mouse_poll(uint8_t *buttons, int16_t *dx, int16_t *dy)
{
    if (!ps2_mouse_inited) return 0;

    while (c4pros_ps2_mouse_byte_ready()) {
        uint8_t b = ps2_read_data();
        if (ps2_mouse_packet_idx == 0u) {
            /* Только байты с битом 3 считаем началом пакета мыши (MS format) */
            if (!(b & 0x08u)) continue;
        }
        ps2_mouse_packet_buf[ps2_mouse_packet_idx++] = b;

        if (ps2_mouse_packet_idx >= 3u) {
            ps2_mouse_packet_idx = 0;
            uint8_t f = ps2_mouse_packet_buf[0];
            uint8_t dx8 = ps2_mouse_packet_buf[1];
            uint8_t dy8 = ps2_mouse_packet_buf[2];
            /* Расширение знака для 9-битных дельт */
            int16_t dx16 = (int16_t)((f & 0x10u) ? (dx8 | 0xFF00u) : dx8);
            int16_t dy16 = (int16_t)((f & 0x20u) ? (dy8 | 0xFF00u) : dy8);
            if (buttons) *buttons = f & 0x07u;
            if (dx) *dx = dx16;
            if (dy) *dy = dy16;
            return 1;
        }
    }
    return 0;
}

/* Сбросить выравнивание пакетов (если потеряли байт). */
void c4pros_ps2_mouse_reset_packet_state(void)
{
    ps2_mouse_packet_idx = 0;
}

/* --- Мышь: INT 33h (драйвер) или INT 15h (callback) --- */
extern int c4pros_mouse_int15_init(void);
extern void c4pros_mouse_int15_enable(void);
extern void c4pros_mouse_int15_disable(void);
extern volatile uint8_t mouse_event_ready;
extern int16_t mouse_dx;
extern int16_t mouse_dy;
extern uint8_t mouse_buttons;

static int mouse_use_int33;           /* 1 = опрос через INT 33h AX=3 */
static int16_t mouse_last_x, mouse_last_y;
static int mouse_first_poll;          /* первый poll после init */
static int mouse_int33_probed;        /* уже пробовали INT 33h AX=3 в poll */
static unsigned int mouse_zero_count; /* подряд (0,0) от INT 33h — переключиться на INT 15h */

/* INT 33h AX=0: сброс, возврат AX=0xFFFF если драйвер есть */
static int mouse_int33_available(void)
{
    uint16_t ax;
    __asm__ volatile ("movw $0, %%ax; int $0x33" : "=a" (ax) : : "bx", "cx", "dx", "cc");
    return (ax == 0xFFFF);
}

/* Один раз в poll пробуем INT 33h AX=3 — в части сред драйвер есть, но AX=0 не 0xFFFF */
static int mouse_try_int33_once(uint8_t *buttons, int16_t *dx, int16_t *dy)
{
    uint16_t bx, cx, dx_val;
    __asm__ volatile ("movw $3, %%ax; int $0x33"
        : "=b" (bx), "=c" (cx), "=d" (dx_val) : : "ax", "cc");
    /* координаты обычно 0..639/0..479 или 0..1023; мусор чаще большой */
    if (cx > 2047u || dx_val > 2047u) return 0;
    mouse_use_int33 = 1;
    mouse_first_poll = 1;
    mouse_last_x = (int16_t)cx;
    mouse_last_y = (int16_t)dx_val;
    mouse_first_poll = 0;
    if (buttons) *buttons = (uint8_t)(bx & 0x07u);
    if (dx) *dx = 0;
    if (dy) *dy = 0;
    return 1;
}

int c4pros_mouse_init(void)
{
    mouse_use_int33 = 0;
    mouse_first_poll = 1;
    mouse_int33_probed = 0;
    mouse_zero_count = 0;

    if (mouse_int33_available()) {
        mouse_use_int33 = 1;
        return 0;
    }
    return c4pros_mouse_int15_init();
}

void c4pros_mouse_enable(void)
{
    if (mouse_use_int33) {
        /* INT 33h AX=1 — включить курсор/отслеживание (некоторые драйверы без этого дают 0,0) */
        __asm__ volatile ("movw $1, %%ax; int $0x33" : : : "ax", "bx", "cx", "dx", "cc");
    } else {
        mouse_dx = 320;
        mouse_dy = 240;
        c4pros_mouse_int15_enable();
    }
}

void c4pros_mouse_disable(void)
{
    if (!mouse_use_int33)
        c4pros_mouse_int15_disable();
}

int c4pros_mouse_returns_absolute(void)
{
    return mouse_use_int33;
}

int c4pros_mouse_poll(uint8_t *buttons, int16_t *dx, int16_t *dy)
{
    if (mouse_use_int33) {
        uint16_t bx, cx, dx_val;
        __asm__ volatile ("movw $3, %%ax; int $0x33"
            : "=b" (bx), "=c" (cx), "=d" (dx_val) : : "ax", "cc");
        /* Мусор от драйвера (напр. X:-15865) — сразу на INT 15h */
        if (cx > 1023u || dx_val > 1023u || (int16_t)cx < 0 || (int16_t)dx_val < 0) {
            mouse_use_int33 = 0;
            mouse_dx = 320;
            mouse_dy = 240;
            mouse_event_ready = 0;
            if (c4pros_mouse_int15_init() == 0) {
                c4pros_mouse_int15_enable();
                if (buttons) *buttons = 0;
                if (dx) *dx = 320;
                if (dy) *dy = 240;
                return 1;
            }
            mouse_use_int33 = 1; /* не удалось — остаёмся на INT 33h */
        }
        if (buttons) *buttons = (uint8_t)(bx & 0x07u);
        if (dx) *dx = (int16_t)cx;
        if (dy) *dy = (int16_t)dx_val;
        /* Долго только нули — переключаемся на INT 15h */
        if (cx == 0 && dx_val == 0) {
            mouse_zero_count++;
            if (mouse_zero_count > 100u) {
                mouse_use_int33 = 0;
                mouse_zero_count = 0;
                mouse_dx = 320;
                mouse_dy = 240;
                mouse_event_ready = 0;
                if (c4pros_mouse_int15_init() == 0)
                    c4pros_mouse_int15_enable();
            }
        } else
            mouse_zero_count = 0;
        return 1;
    }

    if (mouse_event_ready) {
        if (buttons) *buttons = mouse_buttons;
        if (dx) *dx = mouse_dx;
        if (dy) *dy = mouse_dy;
        mouse_event_ready = 0;
        return 1;
    }
    /* INT 15h callback не сработал — один раз пробуем INT 33h AX=3 */
    if (!mouse_int33_probed) {
        mouse_int33_probed = 1;
        if (mouse_try_int33_once(buttons, dx, dy)) return 1;
    }
    return 0;
}
