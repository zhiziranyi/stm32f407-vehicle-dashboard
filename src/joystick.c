#include "joystick.h"
#include "stm32f4xx_hal.h"
#include <stdlib.h>

/* Pins: PA4=VRx, PA5=VRy, PA6=SW */
#define JOY_X_PIN   GPIO_PIN_4
#define JOY_X_PORT  GPIOA
#define JOY_Y_PIN   GPIO_PIN_5
#define JOY_Y_PORT  GPIOA
#define JOY_SW_PIN  GPIO_PIN_6
#define JOY_SW_PORT GPIOA

#define JOY_CENTER    1800
#define JOY_DEADZONE  700
#define SW_DEBOUNCE   30
#define SW_LONG       500

static ADC_HandleTypeDef hadc1;
static uint8_t g_joy_disabled = 0;  /* 浮空检测: 1=禁用摇杆 */
static uint32_t adc_read(uint32_t ch);  /* forward */

void joystick_init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* Analog: PA4, PA5 */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = JOY_X_PIN | JOY_Y_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(JOY_X_PORT, &gpio);

    /* Digital: PA6 */
    gpio.Pin  = JOY_SW_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(JOY_SW_PORT, &gpio);

    /* ADC1 */
    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode          = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    /* 自动检测摇杆是否连接: 连续读10次, 方差过大=浮空 */
    uint32_t samples_x[10], samples_y[10];
    uint32_t sum_x = 0, sum_y = 0, var_x = 0, var_y = 0;
    for (int i = 0; i < 10; i++) {
        samples_x[i] = adc_read(ADC_CHANNEL_4);
        samples_y[i] = adc_read(ADC_CHANNEL_5);
        sum_x += samples_x[i]; sum_y += samples_y[i];
        HAL_Delay(5);
    }
    uint32_t avg_x = sum_x / 10, avg_y = sum_y / 10;
    for (int i = 0; i < 10; i++) {
        int32_t dx = (int32_t)(samples_x[i] - avg_x);
        int32_t dy = (int32_t)(samples_y[i] - avg_y);
        var_x += (uint32_t)(dx * dx);
        var_y += (uint32_t)(dy * dy);
    }
    var_x /= 10; var_y /= 10;
    /* 方差 > 50000 说明引脚浮空 (正常接摇杆时 < 500) */
    if (var_x > 50000 || var_y > 50000) {
        g_joy_disabled = 1;
    }
}

static uint32_t adc_read(uint32_t ch)
{
    ADC_ChannelConfTypeDef s = {0};
    s.Channel      = ch;
    s.Rank         = 1;
    s.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &s);
    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return JOY_CENTER;
    }
    uint32_t v = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return v;
}

uint8_t joystick_read(void)
{
    if (g_joy_disabled) return JOY_NONE;

    uint32_t now = HAL_GetTick();

    /* ---- ADC direction (winner-take-all, 200 deadzone) ---- */
    int16_t dy_axis = (int16_t)(adc_read(ADC_CHANNEL_4) - JOY_CENTER); /* X=PA4=up/down */
    int16_t dx_axis = (int16_t)(adc_read(ADC_CHANNEL_5) - JOY_CENTER); /* Y=PA5=left/right */

    uint8_t dir = JOY_NONE;
    if (abs(dx_axis) > abs(dy_axis)) {
        if      (dx_axis >  JOY_DEADZONE) dir = JOY_RIGHT;
        else if (dx_axis < -JOY_DEADZONE) dir = JOY_LEFT;
    } else {
        if      (dy_axis >  JOY_DEADZONE) dir = JOY_DOWN;
        else if (dy_axis < -JOY_DEADZONE) dir = JOY_UP;
    }

    /* Return direction as edge (once per change) */
    static uint8_t  last_dir = JOY_NONE;
    static uint32_t dir_since;
    if (dir != last_dir) {
        last_dir  = dir;
        dir_since = now;
        if (dir != JOY_NONE) return dir;
    }
    /* Repeat held direction every 250ms */
    if (dir != JOY_NONE && (now - dir_since) > 250) {
        dir_since = now;
        return dir;
    }

    /* ---- SW button (debounced state machine) ---- */
    static uint8_t  sw_db      = 1;
    static uint8_t  sw_last    = 1;
    static uint32_t sw_changed;
    static uint32_t sw_press;
    static uint8_t  sw_short_done;

    uint8_t raw = (HAL_GPIO_ReadPin(JOY_SW_PORT, JOY_SW_PIN) == GPIO_PIN_RESET) ? 0 : 1;

    if (raw != sw_last) { sw_changed = now; sw_last = raw; }
    if ((now - sw_changed) > SW_DEBOUNCE && raw != sw_db) {
        sw_db = raw;
        if (sw_db == 0) {          /* pressed */
            sw_press = now;
            sw_short_done = 0;
        } else {                   /* released → short press */
            if (!sw_short_done) return JOY_SHORT;
        }
    }
    /* Long press while held */
    if (sw_db == 0 && !sw_short_done && (now - sw_press) > SW_LONG) {
        sw_short_done = 1;
        return JOY_LONG;
    }

    return JOY_NONE;
}

void joystick_debug(uint16_t *x, uint16_t *y, uint8_t *sw)
{
    *x  = (uint16_t)adc_read(ADC_CHANNEL_4);
    *y  = (uint16_t)adc_read(ADC_CHANNEL_5);
    *sw = (HAL_GPIO_ReadPin(JOY_SW_PORT, JOY_SW_PIN) == GPIO_PIN_RESET) ? 0 : 1;
}
