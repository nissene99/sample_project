/*
Autor:          Erik Nissen
Studiengang:    Informationstechnologie (INI)
Matrikelnummer: 937388
Datei:          main/server/sntp/config.h
Erstellt:       16.06.2022
*/

#ifndef MAIN_CONFIG_H
#define MAIN_CONFIG_H

RTC_DATA_ATTR static int boot_count = 0;

static void obtain_time(void);

static void initialize_sntp(void);

void sntp_sync_time(struct timeval *tv);

void time_sync_notification_cb(struct timeval *tv);

static void obtain_time(void);

static void initialize_sntp(void);

#endif //MAIN_CONFIG_H
