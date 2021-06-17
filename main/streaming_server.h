/*
 * webserver.h
 *
 *  Created on: Jun 6, 2021
 *      Author: xenir
 */

#ifndef MAIN_STREAMSERVER_H_
#define MAIN_STREAMSERVER_H_


#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_wifi_netif.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"

#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t start_streaming_server();

#ifdef __cplusplus
}
#endif

#endif /* MAIN_STREAMSERVER_H_ */
