#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "wifi.h"

#include "webserver.h"
#include "streaming_server.h"
#include "audio_pipeline.h"
#include "i2s_stream.h"
#include "board.h"
#include "streaming_http_audio.h"

#define BASE_PATH "/spiffs"
static const char *TAG = "esp32-streaming-audio";

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

void audio_process(void)
{
    audio_pipeline_handle_t pipeline;
    audio_element_handle_t i2s_stream_reader, i2s_stream_writer, http_audio;

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "[ 1 ] Start codec chip");
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 2 ] Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);

    ESP_LOGI(TAG, "[3.1a] Create i2s stream to read data from codec chip");

    i2s_stream_cfg_t i2s_cfg_read = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg_read.type = AUDIO_STREAM_READER;
    i2s_cfg_read.i2s_config.sample_rate = 8000;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg_read);

    ESP_LOGI(TAG, "[3.1b] Create i2s stream to write data to codec chip");

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.i2s_config.sample_rate = 8000;
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "[3.2] Create HTTP Streamer");

    streaming_http_audio_cfg_t sha_cfg = DEFAULT_STREAMING_HTTP_AUDIO_CONFIG();
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    memcpy( &(sha_cfg.http_cfg), &config, sizeof(httpd_config_t) );
    sha_cfg.http_cfg.server_port = 8080;
    sha_cfg.http_cfg.ctrl_port = 8081;
    sha_cfg.sample_rate = 8000;
    http_audio = streaming_http_audio_init(&sha_cfg);

    ESP_LOGI(TAG, "[3.3] Register all elements to audio pipeline");

    audio_pipeline_register(pipeline, i2s_stream_reader, "i2s_read");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s_write");
    audio_pipeline_register(pipeline, http_audio, "http_audio");

    ESP_LOGI(TAG, "[3.4] Link it together [codec_chip]-->i2s_stream_reader-->http_audio");


    const char *link_tag[3] = {"i2s_read", "http_audio"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);

/*
    const char *link_tag[3] = {"i2s_read", "i2s_write"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);
*/

    ESP_LOGI(TAG, "[ 4 ] Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "[4.1] Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "[ 5 ] Start audio_pipeline");
    audio_pipeline_run(pipeline);

    ESP_LOGI(TAG, "[ 6 ] Listen for all pipeline events");

    while (1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        /* Stop when the last pipeline element (i2s_stream_writer in this case) receives stop event */
        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *) http_audio
            && msg.cmd == AEL_MSG_CMD_REPORT_STATUS
            && (((int)msg.data == AEL_STATUS_STATE_STOPPED) || ((int)msg.data == AEL_STATUS_STATE_FINISHED))) {
            ESP_LOGW(TAG, "[ * ] Stop event received");
            break;
        }
    }

    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, i2s_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, http_audio);

    /* Terminate the pipeline before removing the listener */
    audio_pipeline_remove_listener(pipeline);

    /* Make sure audio_pipeline_remove_listener & audio_event_iface_remove_listener are called before destroying event_iface */
    audio_event_iface_destroy(evt);

    /* Release all resources */
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(http_audio);
}

void app_main()
{
    ESP_LOGI(TAG, "Initializing Web Server");
	init_webserver();
	audio_process();
}
