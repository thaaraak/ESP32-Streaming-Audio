#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "wifi.h"
#include "webserver.h"

#define BASE_PATH "/spiffs"
static const char *TAG = "esp32-streaming-audio";

extern "C" {
	void app_main();
	void command_callback( const char* command );
}



void command_callback( const char* command, char* response )
{
	ESP_LOGI( TAG, "In command callback: %s\n", command );
}


static esp_err_t init_spiffs(const char *base_path)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = {
      .base_path = base_path,
      .partition_label = NULL,
      .max_files = 5,   // This decides the maximum number of files that can be created on the storage
      .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    return ESP_OK;
}


void init_webserver(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    init_spiffs( BASE_PATH );
    wifi_connect_with_hostname( CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD, CONFIG_ESP_HOSTNAME );
    start_webserver( BASE_PATH, command_callback );
}

void app_main()
{
    ESP_LOGI(TAG, "Initializing Web Server");
	init_webserver();

    while(1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);

    }
}
