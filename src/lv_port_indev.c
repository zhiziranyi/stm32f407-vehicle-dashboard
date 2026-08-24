#include "lv_port_indev.h"
#include "stm32f4xx_hal.h"
#include <stdlib.h>

/* ---- Joystick pins ---- */
#define JOY_X_PIN       GPIO_PIN_0
#define JOY_X_PORT      GPIOA
#define JOY_Y_PIN       GPIO_PIN_1
#define JOY_Y_PORT      GPIOA
#define JOY_BTN_PIN     GPIO_PIN_2
#define JOY_BTN_PORT    GPIOA

/* ---- Timing (matches ESP32 reference) ---- */
#define JOY_CENTER      2048
#define JOY_DEADZONE    200
#define SW_DEBOUNCE_MS  30
#define SW_LONG_PRESS   500

static ADC_HandleTypeDef hadc1;

static void MX_ADC1_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = JOY_X_PIN | JOY_Y_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(JOY_X_PORT, &gpio);

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
}

static uint32_t ADC_ReadChannel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank    = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        return JOY_CENTER;
    }
    uint32_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

/* ---- SW debounce state machine (from ESP32 reference) ---- */
static uint8_t  sw_debounced = 1;   /* HIGH = not pressed */
static uint8_t  sw_last_reading = 1;
static uint32_t sw_last_change;
static uint32_t sw_press_start;
static uint8_t  sw_short_fired;
static uint8_t  sw_long_fired;

static uint8_t sw_get_event(uint32_t now)
{
    uint8_t reading = (HAL_GPIO_ReadPin(JOY_BTN_PORT, JOY_BTN_PIN) == GPIO_PIN_RESET) ? 0 : 1;

    if (reading != sw_last_reading) {
        sw_last_change = now;
        sw_last_reading = reading;
    }

    if ((now - sw_last_change) > SW_DEBOUNCE_MS) {
        if (reading != sw_debounced) {
            sw_debounced = reading;
            if (sw_debounced == 0) {
                /* Pressed */
                sw_press_start = now;
                sw_short_fired = 0;
                sw_long_fired  = 0;
            } else {
                /* Released — short press if no long press yet */
                if (!sw_long_fired && !sw_short_fired) {
                    sw_short_fired = 1;
                    return 1;  /* SW_SHORT */
                }
            }
        }
    }

    /* Long press while held */
    if (sw_debounced == 0 && !sw_long_fired) {
        if ((now - sw_press_start) >= SW_LONG_PRESS) {
            sw_long_fired = 1;
            return 2;  /* SW_LONG */
        }
    }

    return 0;  /* SW_NONE */
}

/* ---- Joystick read callback ---- */
static void indev_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    uint32_t x = ADC_ReadChannel(ADC_CHANNEL_0);
    uint32_t y = ADC_ReadChannel(ADC_CHANNEL_1);
    uint32_t now = HAL_GetTick();

    /* Winner-take-all direction (from ESP32 reference) */
    int16_t dx = (int16_t)(x - JOY_CENTER);
    int16_t dy = (int16_t)(y - JOY_CENTER);

    uint8_t key = 0;
    if (abs(dx) > abs(dy)) {
        if (dx > JOY_DEADZONE)       key = LV_KEY_RIGHT;
        else if (dx < -JOY_DEADZONE) key = LV_KEY_LEFT;
    } else {
        if (dy > JOY_DEADZONE)       key = LV_KEY_NEXT;
        else if (dy < -JOY_DEADZONE) key = LV_KEY_PREV;
    }

    /* SW button (edge-triggered events) */
    uint8_t sw = sw_get_event(now);
    if (sw == 1) key = LV_KEY_ENTER;
    if (sw == 2) key = LV_KEY_ESC;

    if (key) {
        data->state = LV_INDEV_STATE_PR;
        data->key   = key;
    } else {
        data->state = LV_INDEV_STATE_REL;
        data->key   = 0;
    }
}

void lv_port_indev_init(void)
{
    MX_ADC1_Init();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin  = JOY_BTN_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(JOY_BTN_PORT, &gpio);

    sw_last_change = HAL_GetTick();

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = indev_read;
    lv_indev_drv_register(&indev_drv);
}
