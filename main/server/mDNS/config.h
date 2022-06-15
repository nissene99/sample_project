/*
Autor:         Erik Nissen
Studiengang:    Informationstechnologie (INI)
Matrikelnummer: 937388
Datei:          config.hpp
Erstellt:       15.06.2022
*/

#include "mdns.h"

#ifndef MAIN_CONFIG_HPP
#define MAIN_CONFIG_HPP
#define EXAMPLE_BUTTON_GPIO     0

static const char * ip_protocol_str[] = {"V4", "V6", "MAX"};
static const char * if_str[] = {"STA", "AP", "ETH", "MAX"};
const char * MDNS_NAME = "esp32";

#if CONFIG_MDNS_RESOLVE_TEST_SERVICES == 1
static void  query_mdns_host_with_gethostbyname(char * host);
static void  query_mdns_host_with_getaddrinfo(char * host);
#endif


static char * generate_hostname(void);

static void initialise_mdns(void);

static void mdns_print_results(mdns_result_t *results);

static void query_mdns_service(const char * service_name, const
char * proto);

static bool check_and_print_result(mdns_search_once_t *search);

static void query_mdns_hosts_async(const char * host_name);

static void query_mdns_host(const char * host_name);

static void initialise_button(void);

static void check_button(void);

static void mdns_example_task(void *pvParameters);

static char* generate_hostname(void);

static void  query_mdns_host_with_gethostbyname(char * host);

static void  query_mdns_host_with_getaddrinfo(char * host);

#endif //MAIN_CONFIG_HPP
