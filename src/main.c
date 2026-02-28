/*
 * ============================================================
 *  Simple Calculator — STM32F103C8T6 (Blue Pill)
 *  Toolchain : arm-none-eabi-gcc + STM32 HAL (CubeMX-compatible)
 *  Version   : 1.0.0
 * ============================================================
 *
 *  WIRING
 *  ─────────────────────────────────────────────────────────
 *  16x2 LCD (HD44780, 4-bit mode, no I2C backpack)
 *    RS  → PB0
 *    EN  → PB1
 *    D4  → PB4
 *    D5  → PB5
 *    D6  → PB6
 *    D7  → PB7
 *    RW  → GND
 *    V0  → 10kΩ pot wiper (contrast)
 *    VSS → GND | VDD → 3.3V
 *
 *  4x4 Matrix Keypad
 *    Rows    R1-R4 → PA0, PA1, PA2, PA3   (OUTPUT, push-pull)
 *    Columns C1-C4 → PA4, PA5, PA6, PA7   (INPUT,  pull-up)
 *
 *  Keypad Layout
 *    [ 1 ][ 2 ][ 3 ][ + ]
 *    [ 4 ][ 5 ][ 6 ][ - ]
 *    [ 7 ][ 8 ][ 9 ][ * ]
 *    [ C ][ 0 ][ = ][ / ]
 * ============================================================
 */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>

/* ── Forward declarations ─────────────────────────────────── */
static void SystemClock_Config(void);
static void GPIO_Init(void);

/* LCD */
static void LCD_Init(void);
static void LCD_SendNibble(uint8_t nibble, uint8_t rs);
static void LCD_SendByte(uint8_t byte, uint8_t rs);
static void LCD_Command(uint8_t cmd);
static void LCD_Char(char c);
static void LCD_String(const char *str);
static void LCD_SetCursor(uint8_t col, uint8_t row);
static void LCD_Clear(void);

/* Keypad */
static char Keypad_Scan(void);

/* Calculator */
static void Calc_Reset(void);
static void Calc_UpdateDisplay(void);
static double Calc_Evaluate(double a, double b, char op);
static void Calc_FormatNumber(double val, char *buf, uint8_t maxLen);
static void Calc_HandleKey(char key);

/* Delay */
static void delay_us(uint32_t us);
static void delay_ms(uint32_t ms);

/* ── HAL handle (SysTick only, no peripherals needed) ─────── */
//extern uint32_t uwTick;

/* ════════════════════════════════════════════════════════════
 *  GPIO PIN DEFINITIONS
 * ════════════════════════════════════════════════════════════ */

/* LCD — Port B */
#define LCD_RS_PIN   GPIO_PIN_0
#define LCD_EN_PIN   GPIO_PIN_1
#define LCD_D4_PIN   GPIO_PIN_4
#define LCD_D5_PIN   GPIO_PIN_5
#define LCD_D6_PIN   GPIO_PIN_6
#define LCD_D7_PIN   GPIO_PIN_7
#define LCD_PORT     GPIOB

/* Keypad — Port A */
#define KP_R1        GPIO_PIN_0
#define KP_R2        GPIO_PIN_1
#define KP_R3        GPIO_PIN_2
#define KP_R4        GPIO_PIN_3
#define KP_C1        GPIO_PIN_4
#define KP_C2        GPIO_PIN_5
#define KP_C3        GPIO_PIN_6
#define KP_C4        GPIO_PIN_7
#define KP_PORT      GPIOA

static const uint16_t KP_ROWS[4] = {KP_R1, KP_R2, KP_R3, KP_R4};
static const uint16_t KP_COLS[4] = {KP_C1, KP_C2, KP_C3, KP_C4};

static const char KP_MAP[4][4] = {
    {'1', '2', '3', '+'},
    {'4', '5', '6', '-'},
    {'7', '8', '9', '*'},
    {'C', '0', '=', '/'}
};

/* ════════════════════════════════════════════════════════════
 *  CALCULATOR STATE
 * ════════════════════════════════════════════════════════════ */
#define INPUT_BUF_SIZE  12

typedef struct {
    double  operandA;
    double  operandB;
    char    op;
    char    inputBuf[INPUT_BUF_SIZE];
    uint8_t inputLen;
    bool    freshResult;
    bool    errorState;
    bool    hasDecimal;
} CalcState;

static CalcState calc;

/* ════════════════════════════════════════════════════════════
 *  MAIN
 * ════════════════════════════════════════════════════════════ */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    LCD_Init();
    Calc_Reset();

    while (1)
    {
        char key = Keypad_Scan();
        if (key != '\0') {
            Calc_HandleKey(key);
            /* Simple debounce: wait for key release */
            while (Keypad_Scan() != '\0');
            delay_ms(30);
        }
    }
}

/* ════════════════════════════════════════════════════════════
 *  SYSTEM CLOCK — 72 MHz via PLL (HSE 8 MHz)
 * ════════════════════════════════════════════════════════════ */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL     = RCC_PLL_MUL9;    /* 8 MHz × 9 = 72 MHz */
    HAL_RCC_OscConfig(&osc);

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;   /* APB1 max 36 MHz    */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);
}

/* ════════════════════════════════════════════════════════════
 *  GPIO INIT
 * ════════════════════════════════════════════════════════════ */
static void GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* ── LCD pins — PB0,1,4,5,6,7 as push-pull output ──── */
    gpio.Pin   = LCD_RS_PIN | LCD_EN_PIN
               | LCD_D4_PIN | LCD_D5_PIN | LCD_D6_PIN | LCD_D7_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(LCD_PORT, &gpio);

    /* ── Keypad rows PA0-PA3 — push-pull output ─────────── */
    gpio.Pin   = KP_R1 | KP_R2 | KP_R3 | KP_R4;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    gpio.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(KP_PORT, &gpio);

    /* ── Keypad cols PA4-PA7 — input with pull-up ────────── */
    gpio.Pin   = KP_C1 | KP_C2 | KP_C3 | KP_C4;
    gpio.Mode  = GPIO_MODE_INPUT;
    gpio.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(KP_PORT, &gpio);

    /* All rows high (inactive) */
    HAL_GPIO_WritePin(KP_PORT, KP_R1 | KP_R2 | KP_R3 | KP_R4, GPIO_PIN_SET);
}

/* ════════════════════════════════════════════════════════════
 *  DELAY UTILITIES
 * ════════════════════════════════════════════════════════════ */
static void delay_us(uint32_t us)
{
    /* At 72 MHz, one loop iteration ≈ 1/72 µs.
       Multiply by 72 / loop_overhead (empirically ~4 cycles). */
    uint32_t count = us * 18;
    while (count--) __NOP();
}

static void delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/* ════════════════════════════════════════════════════════════
 *  LCD DRIVER — HD44780, 4-bit mode
 * ════════════════════════════════════════════════════════════ */
static inline void LCD_SetDataPins(uint8_t nibble)
{
    HAL_GPIO_WritePin(LCD_PORT, LCD_D4_PIN, (nibble >> 0) & 1);
    HAL_GPIO_WritePin(LCD_PORT, LCD_D5_PIN, (nibble >> 1) & 1);
    HAL_GPIO_WritePin(LCD_PORT, LCD_D6_PIN, (nibble >> 2) & 1);
    HAL_GPIO_WritePin(LCD_PORT, LCD_D7_PIN, (nibble >> 3) & 1);
}

static void LCD_Pulse(void)
{
    HAL_GPIO_WritePin(LCD_PORT, LCD_EN_PIN, GPIO_PIN_SET);
    delay_us(1);
    HAL_GPIO_WritePin(LCD_PORT, LCD_EN_PIN, GPIO_PIN_RESET);
    delay_us(50);
}

static void LCD_SendNibble(uint8_t nibble, uint8_t rs)
{
    HAL_GPIO_WritePin(LCD_PORT, LCD_RS_PIN, rs ? GPIO_PIN_SET : GPIO_PIN_RESET);
    LCD_SetDataPins(nibble & 0x0F);
    LCD_Pulse();
}

static void LCD_SendByte(uint8_t byte, uint8_t rs)
{
    LCD_SendNibble(byte >> 4, rs);   /* high nibble first */
    LCD_SendNibble(byte & 0x0F, rs); /* low nibble        */
    delay_us(50);
}

static void LCD_Command(uint8_t cmd)  { LCD_SendByte(cmd,  0); }
static void LCD_Char(char c)          { LCD_SendByte((uint8_t)c, 1); }

static void LCD_String(const char *str)
{
    while (*str) LCD_Char(*str++);
}

static void LCD_SetCursor(uint8_t col, uint8_t row)
{
    uint8_t addr = (row == 0) ? (0x80 + col) : (0xC0 + col);
    LCD_Command(addr);
}

static void LCD_Clear(void)
{
    LCD_Command(0x01);
    delay_ms(2);
}

static void LCD_Init(void)
{
    delay_ms(50);                  /* Power-on delay              */

    /* 3× send 0x3 in 8-bit mode to reset the display interface  */
    HAL_GPIO_WritePin(LCD_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    LCD_SendNibble(0x03, 0); delay_ms(5);
    LCD_SendNibble(0x03, 0); delay_us(150);
    LCD_SendNibble(0x03, 0); delay_us(150);

    /* Switch to 4-bit mode */
    LCD_SendNibble(0x02, 0); delay_us(150);

    LCD_Command(0x28); /* Function set: 4-bit, 2-line, 5×8 font  */
    LCD_Command(0x0C); /* Display ON, cursor OFF, blink OFF       */
    LCD_Command(0x06); /* Entry mode: increment, no shift         */
    LCD_Command(0x01); /* Clear display                           */
    delay_ms(2);
}

/* ════════════════════════════════════════════════════════════
 *  KEYPAD DRIVER — row-scanning with pull-up columns
 * ════════════════════════════════════════════════════════════ */
static char Keypad_Scan(void)
{
    for (uint8_t r = 0; r < 4; r++)
    {
        /* Drive the current row LOW, all others HIGH */
        for (uint8_t i = 0; i < 4; i++)
            HAL_GPIO_WritePin(KP_PORT, KP_ROWS[i],
                              (i == r) ? GPIO_PIN_RESET : GPIO_PIN_SET);

        delay_us(10); /* settle */

        for (uint8_t c = 0; c < 4; c++)
        {
            if (HAL_GPIO_ReadPin(KP_PORT, KP_COLS[c]) == GPIO_PIN_RESET)
            {
                /* Restore rows */
                HAL_GPIO_WritePin(KP_PORT,
                                  KP_R1 | KP_R2 | KP_R3 | KP_R4,
                                  GPIO_PIN_SET);
                return KP_MAP[r][c];
            }
        }
    }

    HAL_GPIO_WritePin(KP_PORT, KP_R1 | KP_R2 | KP_R3 | KP_R4, GPIO_PIN_SET);
    return '\0';
}

/* ════════════════════════════════════════════════════════════
 *  NUMBER FORMATTER
 *  Fits the result into maxLen characters for the LCD.
 * ════════════════════════════════════════════════════════════ */
static void Calc_FormatNumber (double val, char *buf, uint8_t maxLen)
{
    if (isnan(val) || isinf(val)) {
        strncpy(buf, "ERR", maxLen);
        return;
    }
    if (fabs(val) >= 1e10) {
        strncpy(buf, "OVERFLOW", maxLen);
        return;
    }

    /* Integer case */
    if (val == (double)(long long)val && fabs(val) < 1e10) {
        snprintf(buf, maxLen, "%lld", (long long)val);
        return;
    }

    /* Float: use up to 6 decimal places, strip trailing zeros */
    snprintf(buf, maxLen, "%.6f", val);
    char *dot = strchr(buf, '.');
    if (dot) {
        char *end = buf + strlen(buf) - 1;
        while (end > dot && *end == '0') *end-- = '\0';
        if (*end == '.')                 *end   = '\0';
    }
}

/* ════════════════════════════════════════════════════════════
 *  DISPLAY REFRESH
 * ════════════════════════════════════════════════════════════ */
static void Calc_UpdateDisplay(void)
{
    char topRow[17]    = {0};
    char bottomRow[17] = {0};

    /* Top row: "operandA  op" */
    if (calc.op != '\0') {
        char numBuf[12];
        Calc_FormatNumber(calc.operandA, numBuf, sizeof(numBuf));
        snprintf(topRow, sizeof(topRow), "%-14s %c", numBuf, calc.op);
    }

    /* Bottom row: live input or current operandA */
    if (calc.inputLen > 0) {
        strncpy(bottomRow, calc.inputBuf, 16);
    } else {
        Calc_FormatNumber(calc.operandA, bottomRow, 16);
    }

    LCD_Clear();
    LCD_SetCursor(0, 0); LCD_String(topRow);
    LCD_SetCursor(0, 1); LCD_String(bottomRow);
}

/* ════════════════════════════════════════════════════════════
 *  ARITHMETIC ENGINE
 * ════════════════════════════════════════════════════════════ */
static double Calc_Evaluate(double a, double b, char op)
{
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/':
            if (b == 0.0) { calc.errorState = true; return 0.0; }
            return a / b;
    }
    return a;
}

/* ════════════════════════════════════════════════════════════
 *  RESET
 * ════════════════════════════════════════════════════════════ */
static void Calc_Reset(void)
{
    memset(&calc, 0, sizeof(CalcState));
    LCD_Clear();
    LCD_SetCursor(0, 0); LCD_String("  STM32 Calc  ");
    LCD_SetCursor(0, 1); LCD_String("   v1.0.0     ");
    delay_ms(1500);
    LCD_Clear();
    LCD_SetCursor(0, 1); LCD_String("0");
}

/* ════════════════════════════════════════════════════════════
 *  KEY HANDLER — core FSM
 * ════════════════════════════════════════════════════════════ */
static void Calc_HandleKey(char key)
{
    /* ── Clear ─────────────────────────────────────────────── */
    if (key == 'C') { Calc_Reset(); return; }

    /* ── Ignore input after error ──────────────────────────── */
    if (calc.errorState) return;

    /* ── Digit ─────────────────────────────────────────────── */
    if ((key >= '0' && key <= '9') || key == '.')
    {
        /* Decimal point guard */
        if (key == '.' && calc.hasDecimal) return;
        if (key == '.') calc.hasDecimal = true;

        if (calc.freshResult) {
            /* Begin new expression after a result */
            calc.operandA    = 0;
            calc.op          = '\0';
            calc.inputLen    = 0;
            calc.hasDecimal  = false;
            calc.freshResult = false;
            memset(calc.inputBuf, 0, INPUT_BUF_SIZE);
        }

        if (calc.inputLen < INPUT_BUF_SIZE - 1) {
            calc.inputBuf[calc.inputLen++] = key;
            calc.inputBuf[calc.inputLen]   = '\0';
        }

        Calc_UpdateDisplay();
        return;
    }

    /* ── Operator ──────────────────────────────────────────── */
    if (key == '+' || key == '-' || key == '*' || key == '/')
    {
        if (calc.inputLen > 0) {
            double entered = atof(calc.inputBuf);

            if (calc.op != '\0') {
                /* Chain: evaluate pending op */
                calc.operandA = Calc_Evaluate(calc.operandA, entered, calc.op);
                if (calc.errorState) {
                    LCD_Clear();
                    LCD_SetCursor(0, 0); LCD_String("  Div by zero!");
                    LCD_SetCursor(0, 1); LCD_String("  Press C     ");
                    return;
                }
            } else {
                calc.operandA = entered;
            }

            calc.inputLen   = 0;
            calc.hasDecimal = false;
            memset(calc.inputBuf, 0, INPUT_BUF_SIZE);
        }

        calc.op          = key;
        calc.freshResult = false;
        Calc_UpdateDisplay();
        return;
    }

    /* ── Equals ────────────────────────────────────────────── */
    if (key == '=')
    {
        if (calc.op == '\0' || calc.inputLen == 0) return;

        calc.operandB = atof(calc.inputBuf);
        double result = Calc_Evaluate(calc.operandA, calc.operandB, calc.op);

        if (calc.errorState) {
            LCD_Clear();
            LCD_SetCursor(0, 0); LCD_String("  Div by zero!");
            LCD_SetCursor(0, 1); LCD_String("  Press C     ");
            return;
        }
        if (fabs(result) >= 1e10) {
            LCD_Clear();
            LCD_SetCursor(0, 0); LCD_String("   Overflow!  ");
            LCD_SetCursor(0, 1); LCD_String("  Press C     ");
            calc.errorState = true;
            return;
        }

        char resBuf[17] = {0};
        Calc_FormatNumber(result, resBuf, 16);

        LCD_Clear();
        LCD_SetCursor(0, 0); LCD_String("= ");
        LCD_String(resBuf);
        LCD_SetCursor(0, 1); LCD_String(resBuf);

        calc.operandA    = result;
        calc.op          = '\0';
        calc.inputLen    = 0;
        calc.hasDecimal  = false;
        calc.freshResult = true;
        memset(calc.inputBuf, 0, INPUT_BUF_SIZE);
        return;
    }
}

/* ════════════════════════════════════════════════════════════
 *  HAL REQUIRED CALLBACKS
 * ════════════════════════════════════════════════════════════ */
void SysTick_Handler(void)
{
    HAL_IncTick();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { /* trap */ }
}
