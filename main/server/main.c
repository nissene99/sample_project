/*
Autor:          Erik Nissen
Studiengang:    Informationstechnologie (INI)
Matrikelnummer: 937388
Datei:          main/server/main.c
Erstellt:       15.06.2022
*/

#include "config.h"
#include "mDNS/mDNS.c"
#include "sntp/sntp.c"
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>


static char *http_auth_basic(const char *username, const char *password)
{
	int out;
	char *user_info = NULL;
	char *digest = NULL;
	size_t n = 0;
	asprintf(&user_info, "%s:%s", username, password);
	if (!user_info) {
		ESP_LOGE("http", "No enough memory for user information");
		return NULL;
	}
	esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

	/* 6: The length of the "Basic " string
	 * n: Number of bytes for a base64 encode format
	 * 1: Number of bytes for a reserved which be used to fill zero
	*/
	digest = calloc(1, 6 + n + 1);
	if (digest) {
		strcpy(digest, "Basic ");
		esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
	}
	free(user_info);
	return digest;
}

/* An HTTP GET handler */
static esp_err_t basic_auth_get_handler(httpd_req_t *req)
{
	char *buf = NULL;
	size_t buf_len = 0;
	basic_auth_info_t *basic_auth_info = req->user_ctx;

	buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
	if (buf_len > 1) {
		buf = calloc(1, buf_len);
		if (!buf) {
			ESP_LOGE("http", "No enough memory for basic authorization");
			return ESP_ERR_NO_MEM;
		}

		if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
			ESP_LOGI("http", "Found header => Authorization: %s", buf);
		} else {
			ESP_LOGE("http", "No auth value received");
		}

		char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
		if (!auth_credentials) {
			ESP_LOGE("http", "No enough memory for basic authorization credentials");
			free(buf);
			return ESP_ERR_NO_MEM;
		}

		if (strncmp(auth_credentials, buf, buf_len)) {
			ESP_LOGE("http", "Not authenticated");
			httpd_resp_set_status(req, HTTPD_401);
			httpd_resp_set_type(req, "application/json");
			httpd_resp_set_hdr(req, "Connection", "keep-alive");
			httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
			httpd_resp_send(req, NULL, 0);
		} else {
			ESP_LOGI("http", "Authenticated!");
			char *basic_auth_resp = NULL;
			httpd_resp_set_status(req, HTTPD_200);
			httpd_resp_set_type(req, "application/json");
			httpd_resp_set_hdr(req, "Connection", "keep-alive");
			asprintf(&basic_auth_resp, "{\"authenticated\": true,\"user\": \"%s\"}", basic_auth_info->username);
			if (!basic_auth_resp) {
				ESP_LOGE("http", "No enough memory for basic authorization response");
				free(auth_credentials);
				free(buf);
				return ESP_ERR_NO_MEM;
			}
			httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
			free(basic_auth_resp);
		}
		free(auth_credentials);
		free(buf);
	} else {
		ESP_LOGE("http", "No auth header received");
		httpd_resp_set_status(req, HTTPD_401);
		httpd_resp_set_type(req, "application/json");
		httpd_resp_set_hdr(req, "Connection", "keep-alive");
		httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
		httpd_resp_send(req, NULL, 0);
	}

	return ESP_OK;
}

static void httpd_register_basic_auth(httpd_handle_t server)
{
	basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
	if (basic_auth_info) {
		basic_auth_info->username = AUTH_USERNAME;
		basic_auth_info->password = AUTH_PASSWORD;

		basic_auth.user_ctx = basic_auth_info;
		httpd_register_uri_handler(server, &basic_auth);
	}
}

/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
	char*  buf;
	size_t buf_len;

	/* Get header value string length and allocate memory for length + 1,
	 * extra byte for null termination */
	buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
	if (buf_len > 1) {
		buf = malloc(buf_len);
		/* Copy null terminated value string into buffer */
		if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
			ESP_LOGI("http", "Found header => Host: %s", buf);
		}
		free(buf);
	}

	buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
	if (buf_len > 1) {
		buf = malloc(buf_len);
		if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK) {
			ESP_LOGI("http", "Found header => Test-Header-2: %s", buf);
		}
		free(buf);
	}

	buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
	if (buf_len > 1) {
		buf = malloc(buf_len);
		if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK) {
			ESP_LOGI("http", "Found header => Test-Header-1: %s", buf);
		}
		free(buf);
	}

	/* Read URL query string length and allocate memory for length + 1,
	 * extra byte for null termination */
	buf_len = httpd_req_get_url_query_len(req) + 1;
	if (buf_len > 1) {
		buf = malloc(buf_len);
		if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
			ESP_LOGI("http", "Found URL query => %s", buf);
			char param[32];
			/* Get value of expected key from query string */
			if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK) {
				ESP_LOGI("http", "Found URL query parameter => query1=%s", param);
			}
			if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK) {
				ESP_LOGI("http", "Found URL query parameter => query3=%s", param);
			}
			if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK) {
				ESP_LOGI("http", "Found URL query parameter => query2=%s", param);
			}
		}
		free(buf);
	}

	/* Set some custom headers */
	httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
	httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

	/* Send response with custom headers and body set as the
	 * string passed in user context*/
	const char* resp_str = (const char*) req->user_ctx;
	httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

	/* After sending the HTTP response the old HTTP request
	 * headers are lost. Check if HTTP request headers can be read now. */
	if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
		ESP_LOGI("http", "Request headers lost");
	}
	return ESP_OK;
}

static esp_err_t echo_post_handler(httpd_req_t *req)
{
	char buf[100];
	int ret, remaining = req->content_len;

	while (remaining > 0) {
		/* Read the data for the request */
		if ((ret = httpd_req_recv(req, buf,
		                          MIN(remaining, sizeof(buf)))) <= 0) {
			if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
				/* Retry receiving if timeout occurred */
				continue;
			}
			return ESP_FAIL;
		}

		/* Send back the same data */
		httpd_resp_send_chunk(req, buf, ret);
		remaining -= ret;

		/* Log data received */
		ESP_LOGI("http", "=========== RECEIVED DATA ==========");
		ESP_LOGI("http", "%.*s", ret, buf);
		ESP_LOGI("http", "====================================");
	}

	// End response
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
	if (strcmp("/hello", req->uri) == 0) {
		httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
		/* Return ESP_OK to keep underlying socket open */
		return ESP_OK;
	} else if (strcmp("/echo", req->uri) == 0) {
		httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
		/* Return ESP_FAIL to close underlying socket */
		return ESP_FAIL;
	}
	/* For any other URI send 404 and close socket */
	httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
	return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
	char buf = 0;
	int ret;

	if ((ret = httpd_req_recv(req, &buf, 3)) <= 0) {
		if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
			httpd_resp_send_408(req);
		}
		return ESP_FAIL;
	}

	if ( buf != 0  && buf <= 100) {
		xQueueSend(globalQueue, &buf, portMAX_DELAY);
		ESP_LOGI("dimmer", "Change LED dim to %d%%.", buf);
	}

	/* Respond with empty body */
	httpd_resp_send(req, NULL, 0);
	return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
	httpd_handle_t server = NULL;
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.lru_purge_enable = true;

	// Start the httpd server
	ESP_LOGI("http", "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		// Set URI handlers
		ESP_LOGI("http", "Registering URI handlers");
		httpd_register_uri_handler(server, &hello);
		httpd_register_uri_handler(server, &dimmer);
#if CONFIG_EXAMPLE_BASIC_AUTH
		httpd_register_basic_auth(server);
#endif
		return server;
	}

	ESP_LOGI("http", "Error starting server!");
	return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
	// Stop the httpd server
	return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
	httpd_handle_t* server = (httpd_handle_t*) arg;
	if (*server) {
		ESP_LOGI("http", "Stopping webserver");
		if (stop_webserver(*server) == ESP_OK) {
			*server = NULL;
		} else {
			ESP_LOGE("http", "Failed to stop http server");
		}
	}
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
	httpd_handle_t* server = (httpd_handle_t*) arg;
	if (*server == NULL) {
		ESP_LOGI("http", "Starting webserver");
		*server = start_webserver();
	}
}


void http_server_task(void* arg)
{
	TaskHandle_t mDNS_handle;
	httpd_handle_t server = NULL;

	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	server = start_webserver();
	xTaskCreate(mdns_task, "mDNS", 2048, NULL, 3, &mDNS_handle);
	for(;;){
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
}