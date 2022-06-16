/*
Autor:          Erik Nissen
Studiengang:    Informationstechnologie (INI)
Matrikelnummer: 937388
Datei:          main/server/sntp/snto.c
Erstellt:       16.06.2022
*/

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sntp.h"
#include "config.h"
#include "connect.c"

static const char *TAG = "sntp";



void sntp_sync_time(struct timeval *tv)
{
	settimeofday(tv, NULL);
	ESP_LOGI( TAG, "Time is synchronized from custom code");
	sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}

void time_sync_notification_cb(struct timeval *tv)
{
	ESP_LOGI( TAG, "Notification of a time synchronization event");
}

static void initialize_sntp(void)
{
	ESP_LOGI( TAG, "Initializing SNTP");
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, "pool.ntp.org");
	sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
	sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
	sntp_init();
}

static void obtain_time(void)
{
	ESP_ERROR_CHECK( nvs_flash_init() );
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK( esp_event_loop_create_default() );

	/**
	 * NTP server address could be aquired via DHCP,
	 * see LWIP_DHCP_GET_NTP_SRV menuconfig option
	 */
#ifdef LWIP_DHCP_GET_NTP_SRV
	sntp_servermode_dhcp(1);
#endif

	/* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
	 * Read "Establishing Wi-Fi or Ethernet Connection" section in
	 * examples/protocols/README.md for more information about this function.
	 */
	ESP_ERROR_CHECK(example_connect());

	initialize_sntp();

	// wait for time to be set
	time_t now = 0;
	struct tm timeinfo = { 0 };
	int retry = 0;
	const int retry_count = 10;
	while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
		ESP_LOGI( TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
	time(&now);
	localtime_r(&now, &timeinfo);

	ESP_ERROR_CHECK( example_disconnect() );
}