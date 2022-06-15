#include <sys/cdefs.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "sys/time.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "server/main.c"
#include "esp_err.h"
#include "Config.h"
#include <driver/gpio.h>
#include "driver/touch_pad.h"
#include "soc/rtc_periph.h"
#include "soc/sens_periph.h"

#include <string.h>

#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_dpp.h"
#include "nvs_flash.h"



//#include "qrcode.h"

QueueHandle_t xQueue1;
void A_3_task(void *pvParameter)
{
	int counter = 0;
	for ( ;; ) {
		//Log current time formated as "HH:MM:SS"
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ESP_LOGI("AufgabeA", "%02d:%02d:%02d", (int)tv.tv_sec / 3600,
				 (int)tv.tv_sec % 3600 / 60, (int)tv.tv_sec % 60);
		//wait 1 seconds
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		counter++;
		if(counter == CONFIG_SECONDS){
			ESP_LOGW("AufgabeA", "Es sind 10 Sekunden vergangen.");
			//read from queue
			int value;
			xQueueReceive(xQueue1, &value,
						  portMAX_DELAY);
			ESP_LOGI("AufgabeA 12.", "Der Wert aus der Queue ist: %d",
					 value);
			counter = 0;
		}
	}
}

void A_9_task(void *pvParameter){
	int param;

	for(;;){
		param = (int)pvParameter;
		xQueueSend(xQueue1, &param, portMAX_DELAY);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

}

void AufgabeA(){
	TaskHandle_t a3_task_handle, a9_task_handle;

	xQueue1 = xQueueCreate(10, sizeof(int));

	//pin the task to core 0
	xTaskCreatePinnedToCore(A_3_task, "AufgabeA 3",
							2048, NULL,
							5,&a3_task_handle,
							0);
	//pin the task to core 1
	xTaskCreatePinnedToCore(A_9_task, "AufgabeA 9",
							2048, (void*)1,
							5,&a9_task_handle,
							1);
}

static bool cb_ledc_fade_end_event(const ledc_cb_param_t *param,
                                   void	*user_arg){
	portBASE_TYPE taskAwoken = pdFALSE;

	if(param->event == LEDC_FADE_END_EVT){
		SemaphoreHandle_t counting_sem = (SemaphoreHandle_t)
				user_arg;
		xSemaphoreGiveFromISR(counting_sem, &taskAwoken);
	}
	return (taskAwoken == pdTRUE);
}

static const char *TAG_PAD = "Touch pad";
static bool s_pad_activated[TOUCH_PAD_MAX];
static uint32_t s_pad_init_val[TOUCH_PAD_MAX];
static void tp_example_set_thresholds(void)
{
	uint16_t touch_value;
	for (int i = 0; i < TOUCH_PAD_MAX; i++) {
		//read filtered value
		touch_pad_read_filtered(i, &touch_value);
		s_pad_init_val[i] = touch_value;
		ESP_LOGI(TAG, "test init: touch pad [%d] val is %d", i, touch_value);
		//set interrupt threshold.
		ESP_ERROR_CHECK(touch_pad_set_thresh(i, touch_value * 2 / 3));

	}
}
static void tp_example_read_task(void *pvParameter)
{
	static int show_message;
	int change_mode = 0;
	int filter_mode = 0;
	while (1) {
		if (filter_mode == 0) {
			//interrupt mode, enable touch interrupt
			touch_pad_intr_enable();
			for (int i = 0; i < TOUCH_PAD_MAX; i++) {
				if (s_pad_activated[i] == true) {
					ESP_LOGI(TAG, "T%d activated!", i);
					// Wait a while for the pad being released
					vTaskDelay(200 / portTICK_PERIOD_MS);
					// Clear information on pad activation
					s_pad_activated[i] = false;
					// Reset the counter triggering a message
					// that application is running
					show_message = 1;
				}
			}
		} else {
			//filter mode, disable touch interrupt
			touch_pad_intr_disable();
			touch_pad_clear_status();
			for (int i = 0; i < TOUCH_PAD_MAX; i++) {
				uint16_t value = 0;
				touch_pad_read_filtered(i, &value);
				if (value < s_pad_init_val[i] * TOUCH_THRESH_PERCENT / 100) {
					ESP_LOGI(TAG, "T%d activated!", i);
					ESP_LOGI(TAG, "value: %d; init val: %d", value, s_pad_init_val[i]);
					vTaskDelay(200 / portTICK_PERIOD_MS);
					// Reset the counter to stop changing mode.
					change_mode = 1;
					show_message = 1;
				}
			}
		}

		vTaskDelay(10 / portTICK_PERIOD_MS);

		// If no pad is touched, every couple of seconds, show a message
		// that application is running
		if (show_message++ % 500 == 0) {
			ESP_LOGI(TAG, "Waiting for any pad being touched...");
		}
		// Change mode if no pad is touched for a long time.
		// We can compare the two different mode.
		if (change_mode++ % 2000 == 0) {
			filter_mode = !filter_mode;
			ESP_LOGW(TAG, "Change mode...%s", filter_mode == 0 ? "interrupt mode" : "filter mode");
		}
	}
}
static void tp_example_rtc_intr(void *arg)
{
	uint32_t pad_intr = touch_pad_get_status();
	//clear interrupt
	touch_pad_clear_status();
	for (int i = 0; i < TOUCH_PAD_MAX; i++) {
		if ((pad_intr >> i) & 0x01) {
			s_pad_activated[i] = true;
		}
	}
}
static void tp_example_touch_pad_init(void)
{
	for (int i = 0; i < TOUCH_PAD_MAX; i++) {
		//init RTC IO and mode for touch pad.
		touch_pad_config(i, TOUCH_THRESH_NO_USE);
	}
}
void B_3_task(void *pvParameter){
	// Initialize touch pad peripheral, it will start a timer to run a filter
	ESP_LOGI(TAG, "Initializing touch pad");
	ESP_ERROR_CHECK(touch_pad_init());
	// If use interrupt trigger mode, should set touch sensor FSM mode at 'TOUCH_FSM_MODE_TIMER'.
	touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
	// Set reference voltage for charging/discharging
	// For most usage scenarios, we recommend using the following combination:
	// the high reference valtage will be 2.7V - 1V = 1.7V, The low reference voltage will be 0.5V.
	touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
	// Init touch pad IO
	tp_example_touch_pad_init();
	// Initialize and start a software filter to detect slight change of capacitance.
	touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
	// Set thresh hold
	tp_example_set_thresholds();
	// Register touch interrupt ISR
	touch_pad_isr_register(tp_example_rtc_intr, NULL);
	// Start a task to show what pads have been touched
	xTaskCreate(&tp_example_read_task, "touch_pad_read_task", 2048, NULL, 5, NULL);
}

void B_2_task(void *pvParameter)
{
	int ch;

	ledc_timer_config(&ledc_timer);

	for(ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
		ledc_channel_config(&ledc_channel[ch]);
	}

	ledc_fade_func_install(0);
	ledc_cbs_t callbacks = {
			.fade_cb = cb_ledc_fade_end_event
	};
	SemaphoreHandle_t counting_sem = xSemaphoreCreateCounting(
			LEDC_TEST_CH_NUM, 0);

	for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
		ledc_cb_register(ledc_channel[ch].speed_mode,
						 ledc_channel[ch].channel,
						 &callbacks,
						 (void *) counting_sem);
	}

	for(;;){
		printf("1. LEDC fade up to duty = %d\n", LEDC_TEST_DUTY);
		for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
			ledc_set_fade_with_time(ledc_channel[ch]
												.speed_mode,
			                        ledc_channel[ch].channel,
									LEDC_TEST_DUTY,
									LEDC_TEST_FADE_TIME
									);
			ledc_fade_start(ledc_channel[ch].speed_mode,
			                ledc_channel[ch].channel,
							LEDC_FADE_NO_WAIT);
		}
		for (int i = 0; i < LEDC_TEST_CH_NUM; i++) {
			xSemaphoreTake(counting_sem, portMAX_DELAY);
		}

		printf("2. LEDC fade down to duty = 0\n");
		for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
			ledc_set_fade_with_time(ledc_channel[ch]
												.speed_mode,
			                        ledc_channel[ch].channel,
									0,
									LEDC_TEST_FADE_TIME
									);
			ledc_fade_start(ledc_channel[ch].speed_mode,
			                ledc_channel[ch].channel,
							LEDC_FADE_NO_WAIT);
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
void AufgabeB(){
	TaskHandle_t b2_task_handle, b3_task_handle;

	xTaskCreatePinnedToCore(B_2_task, "b2_task",
							2048, NULL,
							5,&b2_task_handle,
							1);
}


void AufgabeC(){

}

void app_main(void){
	AufgabeB();
}
