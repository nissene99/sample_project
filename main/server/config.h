/*
Autor:          Erik Nissen
Studiengang:    Informationstechnologie (INI)
Matrikelnummer: 937388
Datei:          main/server/config.h
Erstellt:       15.06.2022
*/



#include <http_server.h>
#include <esp_event_base.h>

#ifndef MAIN_CONFIG_H
#define MAIN_CONFIG_H

#endif //MAIN_CONFIG_H

#define WIFI_SSID ""
#define WIFI_PASSWORD ""
#define AUTH_USERNAME ""
#define AUTH_PASSWORD ""

#define HTTPD_401      "401 UNAUTHORIZED"


static char *http_auth_basic(const char *username, const char
*password);

static esp_err_t basic_auth_get_handler(httpd_req_t *req);

static void httpd_register_basic_auth(httpd_handle_t server);

static esp_err_t hello_get_handler(httpd_req_t *req);

static esp_err_t echo_post_handler(httpd_req_t *req);

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t
err);

static esp_err_t ctrl_put_handler(httpd_req_t *req);

static httpd_handle_t start_webserver(void);

static esp_err_t stop_webserver(httpd_handle_t server);

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data);


typedef struct {
	char *username;
	char *password;
} basic_auth_info_t;

static httpd_uri_t basic_auth = {
		.uri       = "/basic_auth",
		.method    = HTTP_GET,
		.handler   = basic_auth_get_handler,
};

static const httpd_uri_t hello = {
		.uri       = "/hello",
		.method    = HTTP_GET,
		.handler   = hello_get_handler,
		/* Let's pass response string in user
		 * context to demonstrate it's usage */
		.user_ctx  = "Hello World!"
};

static const httpd_uri_t echo = {
		.uri       = "/echo",
		.method    = HTTP_POST,
		.handler   = echo_post_handler,
};

static const httpd_uri_t ctrl = {
		.uri       = "/ctrl",
		.method    = HTTP_PUT,
		.handler   = ctrl_put_handler,
		.user_ctx  = NULL
};

static const httpd_uri_t dimmer = {
		.uri       = "/dimmer",
		.method    = HTTP_PUT,
		.handler   = ctrl_put_handler,
		.user_ctx  = NULL
};