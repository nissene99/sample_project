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


/*
  Read values sensed at all available touch pads.
 Print out values in a loop on a serial monitor.
 */
static void tp_example_read_task(void *pvParameter)
{
	uint16_t touch_value;
	uint16_t touch_filter_value;
#if TOUCH_FILTER_MODE_EN
	printf("Touch Sensor filter mode read, the output format is: \nTouchpad num:[raw data, filtered data]\n\n");
#else
	printf("Touch Sensor normal mode read, the output format is: \nTouchpad num:[raw data]\n\n");
#endif
	while (1) {
		for (int i = 0; i < TOUCH_PAD_MAX; i++) {
#if TOUCH_FILTER_MODE_EN
			// If open the filter mode, please use this API to get the touch pad count.
			touch_pad_read_raw_data(i, &touch_value);
			touch_pad_read_filtered(i, &touch_filter_value);
			// printf("T%d:[%4d,%4d] ", i, touch_value,
			//		touch_filter_value);
			/* if (i == 0) {
				if(touch_filter_value < 1000){
					printf("%d", touch_filter_value);
					// printf("\nein voller Erfolg?\n");
					// printf("\n\n\n"
					//   "**********************"
					//   "\nERFOLG\n"
					//   "**********************"
					//   "\n\n\n");
				}
			} else{
				printf("kein Erfolg - Wet: %d\n", touch_value);
			}
			 */
#else
			touch_pad_read(i, &touch_value);
            printf("T%d:[%4d] ", i, touch_value);
#endif
		}
		printf("\n");
		vTaskDelay(200 / portTICK_PERIOD_MS);
	}
}
static void tp_example_touch_pad_init(void)
{
	for (int i = 0;i< TOUCH_PAD_MAX;i++) {
		touch_pad_config(i, TOUCH_THRESH_NO_USE);
	}
}
void B_3_task(void *pvParameter){
	// Initialize touch pad peripheral.
	// The default fsm mode is software trigger mode.
	ESP_ERROR_CHECK(touch_pad_init());
	// Set reference voltage for charging/discharging
	// In this case, the high reference valtage will be 2.7V - 1V = 1.7V
	// The low reference voltage will be 0.5
	// The larger the range, the larger the pulse count value.
	touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
	tp_example_touch_pad_init();
#if TOUCH_FILTER_MODE_EN
	touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
#endif
	tp_example_read_task(NULL);
	for(;;){
		vTaskDelay(portMAX_DELAY);
	}
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
		//char buf = 0;
		//xQueueReceive(globalQueue, &buf, 50 / portTICK_PERIOD_MS);
		printf("1. LEDC fade up to duty = %d\n", LEDC_TEST_DUTY);
		for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
			//if(buf == 0)
				ledc_set_fade_with_time(ledc_channel[ch]
													.speed_mode,
				                        ledc_channel[ch].channel,
										LEDC_TEST_DUTY,
										LEDC_TEST_FADE_TIME
										);
			/*else
				ledc_set_fade_with_time(ledc_channel[ch]
						                        .speed_mode,
				                        ledc_channel[ch].channel,
				                        (int)(LEDC_TEST_CH_NUM/100 *
										buf),
				                        LEDC_TEST_FADE_TIME
				);*/
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
	TaskHandle_t b2_task_handle;
	TaskHandle_t b3_task_handle;
	globalQueue = xQueueCreate(2, sizeof(char ));
	xTaskCreatePinnedToCore(B_2_task, "b2_task",
							2048, NULL,
							5,&b2_task_handle,
							1);

	/*xTaskCreatePinnedToCore(B_3_task, "b3_task",
	                        2048, NULL,
	                        5,&b3_task_handle,
	                        1);*/
}


void AufgabeC(){
	TaskHandle_t server_handle;
	xTaskCreatePinnedToCore(http_server_task, "http server", 2048,
							NULL, 5, &server_handle, 1);
}

void app_main(void){
	AufgabeA();
}
