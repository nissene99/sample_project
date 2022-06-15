/*
Autor:          Erik Nissen
Studiengang:    Informationstechnologie (INI)
Matrikelnummer: 937388
Datei:          main/Config.h
Erstellt:       15.06.2022
*/

#include "driver/ledc.h"

#ifndef MAIN_CONFIG_H
#define MAIN_CONFIG_H
#endif //MAIN_CONFIG_H

#define LEDC_HS_TIMER          LEDC_TIMER_0
#define LEDC_HS_MODE           LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO       (2) // LED on Board is PIN 2
#define LEDC_HS_CH0_CHANNEL    LEDC_CHANNEL_0
#define LEDC_HS_CH1_GPIO       (19)
#define LEDC_HS_CH1_CHANNEL    LEDC_CHANNEL_1
#define LEDC_LS_TIMER          LEDC_TIMER_1
#define LEDC_LS_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_LS_CH2_GPIO       (4)
#define LEDC_LS_CH2_CHANNEL    LEDC_CHANNEL_2
#define LEDC_LS_CH3_GPIO       (5)
#define LEDC_LS_CH3_CHANNEL    LEDC_CHANNEL_3

#define LEDC_TEST_CH_NUM       (4)
#define LEDC_TEST_DUTY         (4000)
#define LEDC_TEST_FADE_TIME    (3000)

ledc_timer_config_t ledc_timer = {
		.duty_resolution = LEDC_TIMER_13_BIT,
		.freq_hz = 5000,
		.speed_mode = LEDC_HS_MODE,
		.timer_num = LEDC_HS_TIMER,
		.clk_cfg = LEDC_AUTO_CLK,
};

ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
		{
				.channel    = LEDC_HS_CH0_CHANNEL,
				.duty       = 0,
				.gpio_num   = LEDC_HS_CH0_GPIO,
				.speed_mode = LEDC_HS_MODE,
				.hpoint     = 0,
				.timer_sel  = LEDC_HS_TIMER,
				.flags.output_invert = 0
		}
};

#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_THRESH_PERCENT  (80)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (4)

