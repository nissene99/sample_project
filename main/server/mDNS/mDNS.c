/*
Autor:          Erik Nissen
Studiengang:    Informationstechnologie (INI)
Matrikelnummer: 937388
Datei:          main/server/mDNS/mDNS.c
Erstellt:       15.06.2022
*/

#include <lwip/inet.h>
#include <lwip/netdb.h>
#include "config.h"

static void initialise_mdns(void)
{
	char * hostname = generate_hostname();

	//initialize mDNS
	ESP_ERROR_CHECK( mdns_init() );
	//set mDNS hostname (required if you want to advertise services)
	ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
	ESP_LOGI("mDNS", "mdns hostname set to: [%s]", hostname);
	//set default mDNS instance name
	ESP_ERROR_CHECK( mdns_instance_name_set(MDNS_NAME) );

	//structure with TXT records
	mdns_txt_item_t serviceTxtData[3] = {
			{"board", "esp32"},
			{"u", "user"},
			{"p", "password"}
	};

	//initialize service
	ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, 3) );
#if CONFIG_MDNS_MULTIPLE_INSTANCE
	ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer1", "_http", "_tcp", 80, NULL, 0) );
#endif

#if CONFIG_MDNS_PUBLISH_DELEGATE_HOST
	char *delegated_hostname;
    if (-1 == asprintf(&delegated_hostname, "%s-delegated", hostname)) {
        abort();
    }

    mdns_ip_addr_t addr4, addr6;
    esp_netif_str_to_ip4("10.0.0.1", &addr4.addr.u_addr.ip4);
    addr4.addr.type = ESP_IPADDR_TYPE_V4;
    esp_netif_str_to_ip6("fd11:22::1", &addr6.addr.u_addr.ip6);
    addr6.addr.type = ESP_IPADDR_TYPE_V6;
    addr4.next = &addr6;
    addr6.next = NULL;
    ESP_ERROR_CHECK( mdns_delegate_hostname_add(delegated_hostname, &addr4) );
    ESP_ERROR_CHECK( mdns_service_add_for_host("test0", "_http", "_tcp", delegated_hostname, 1234, serviceTxtData, 3) );
    free(delegated_hostname);
#endif // CONFIG_MDNS_PUBLISH_DELEGATE_HOST

	//add another TXT item
	ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "path", "/foobar") );
	//change TXT item value
	ESP_ERROR_CHECK( mdns_service_txt_item_set_with_explicit_value_len("_http", "_tcp", "u", "admin", strlen("admin")) );
	free(hostname);
}

static void mdns_print_results(mdns_result_t *results)
{
	mdns_result_t *r = results;
	mdns_ip_addr_t *a = NULL;
	int i = 1, t;
	while (r) {
		printf("%d: Interface: %s, Type: %s, TTL: %u\n", i++, if_str[r->tcpip_if], ip_protocol_str[r->ip_protocol],
		       r->ttl);
		if (r->instance_name) {
			printf("  PTR : %s.%s.%s\n", r->instance_name, r->service_type, r->proto);
		}
		if (r->hostname) {
			printf("  SRV : %s.local:%u\n", r->hostname, r->port);
		}
		if (r->txt_count) {
			printf("  TXT : [%zu] ", r->txt_count);
			for (t = 0; t < r->txt_count; t++) {
				printf("%s=%s(%d); ", r->txt[t].key, r->txt[t].value ? r->txt[t].value : "NULL", r->txt_value_len[t]);
			}
			printf("\n");
		}
		a = r->addr;
		while (a) {
			if (a->addr.type == ESP_IPADDR_TYPE_V6) {
				printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
			} else {
				printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
			}
			a = a->next;
		}
		r = r->next;
	}
}

static void query_mdns_service(const char * service_name, const char * proto)
{
	ESP_LOGI("mDNS", "Query PTR: %s.%s.local", service_name, proto);

	mdns_result_t * results = NULL;
	esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
	if(err){
		ESP_LOGE("mDNS", "Query Failed: %s", esp_err_to_name(err));
		return;
	}
	if(!results){
		ESP_LOGW("mDNS", "No results found!");
		return;
	}

	mdns_print_results(results);
	mdns_query_results_free(results);
}

static bool check_and_print_result(mdns_search_once_t *search)
{
	// Check if any result is available
	mdns_result_t * result = NULL;
	if (!mdns_query_async_get_results(search, 0, &result)) {
		return false;
	}

	if (!result) {   // search timeout, but no result
		return true;
	}

	// If yes, print the result
	mdns_ip_addr_t * a = result->addr;
	while (a) {
		if(a->addr.type == ESP_IPADDR_TYPE_V6){
			printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
		} else {
			printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
		}
		a = a->next;
	}
	// and free the result
	mdns_query_results_free(result);
	return true;
}

static void query_mdns_hosts_async(const char * host_name)
{
	ESP_LOGI("mDNS", "Query both A and AAA: %s.local", host_name);

	mdns_search_once_t *s_a = mdns_query_async_new(host_name, NULL, NULL, MDNS_TYPE_A, 1000, 1, NULL);
	mdns_query_async_delete(s_a);
	mdns_search_once_t *s_aaaa = mdns_query_async_new(host_name, NULL, NULL, MDNS_TYPE_AAAA, 1000, 1, NULL);
	while (s_a || s_aaaa) {
		if (s_a && check_and_print_result(s_a)) {
			ESP_LOGI("mDNS", "Query A %s.local finished", host_name);
			mdns_query_async_delete(s_a);
			s_a = NULL;
		}
		if (s_aaaa && check_and_print_result(s_aaaa)) {
			ESP_LOGI("mDNS", "Query AAAA %s.local finished", host_name);
			mdns_query_async_delete(s_aaaa);
			s_aaaa = NULL;
		}
	}
}

static void query_mdns_host(const char * host_name)
{
	ESP_LOGI("mDNS", "Query A: %s.local", host_name);

	struct esp_ip4_addr addr;
	addr.addr = 0;

	esp_err_t err = mdns_query_a(host_name, 2000,  &addr);
	if(err){
		if(err == ESP_ERR_NOT_FOUND){
			ESP_LOGW("mDNS", "%s: Host was not found!", esp_err_to_name(err));
			return;
		}
		ESP_LOGE("mDNS", "Query Failed: %s", esp_err_to_name(err));
		return;
	}

	ESP_LOGI("mDNS", "Query A: %s.local resolved to: " IPSTR, host_name, IP2STR(&addr));
}

static void initialise_button(void)
{
	gpio_config_t io_conf = {0};
	io_conf.intr_type = GPIO_INTR_DISABLE;
	io_conf.pin_bit_mask = BIT64(EXAMPLE_BUTTON_GPIO);
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pull_up_en = 1;
	io_conf.pull_down_en = 0;
	gpio_config(&io_conf);
}

static void check_button(void)
{
	static bool old_level = true;
	bool new_level = gpio_get_level(EXAMPLE_BUTTON_GPIO);
	if (!new_level && old_level) {
		query_mdns_hosts_async("esp32-mdns");
		query_mdns_host("esp32");
		query_mdns_service("_arduino", "_tcp");
		query_mdns_service("_http", "_tcp");
		query_mdns_service("_printer", "_tcp");
		query_mdns_service("_ipp", "_tcp");
		query_mdns_service("_afpovertcp", "_tcp");
		query_mdns_service("_smb", "_tcp");
		query_mdns_service("_ftp", "_tcp");
		query_mdns_service("_nfs", "_tcp");
	}
	old_level = new_level;
}

static void mdns_task(void *pvParameters)
{
	while (1) {
		check_button();
		vTaskDelay(50 / portTICK_PERIOD_MS);
	}
}

static char* generate_hostname(void)
{
#ifndef CONFIG_MDNS_ADD_MAC_TO_HOSTNAME
	return strdup(MDNS_NAME);
#else
	uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", CONFIG_MDNS_HOSTNAME, mac[3], mac[4], mac[5])) {
        abort();
    }
    return hostname;
#endif
}

static void  query_mdns_host_with_gethostbyname(char * host)
{
	struct hostent *res = gethostbyname(host);
	if (res) {
		unsigned int i = 0;
		while (res->h_addr_list[i] != NULL) {
			ESP_LOGI("mDNS", "gethostbyname: %s resolved to: %s", host, inet_ntoa(*(struct in_addr *) (res->h_addr_list[i])));
			i++;
		}
	}
}

static void  query_mdns_host_with_getaddrinfo(char * host)
{
	struct addrinfo hints;
	struct addrinfo * res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (!getaddrinfo(host, NULL, &hints, &res)) {
		while (res) {
			ESP_LOGI("mDNS", "getaddrinfo: %s resolved to: %s", host,
			         res->ai_family == AF_INET?
			         inet_ntoa(((struct sockaddr_in *) res->ai_addr)->sin_addr):
			         inet_ntoa(((struct sockaddr_in6 *) res->ai_addr)->sin6_addr));
			res = res->ai_next;
		}
	}
}