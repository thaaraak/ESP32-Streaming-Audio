// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#ifndef _STREAMING_HTTP_AUDIO_H_
#define _STREAMING_HTTP_AUDIO_H_

#include <sys/time.h>

#include "esp_err.h"
#include "audio_element.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief      WAV Encoder configurations
 */
typedef struct {

	int                     out_rb_size;    /*!< Size of output ringbuffer */
    int                     task_stack;     /*!< Task stack size */
    int                     task_core;      /*!< Task running in core (0 or 1) */
    int                     task_prio;      /*!< Task priority (based on freeRTOS priority) */
    bool                    stack_in_ext;   /*!< Try to allocate stack in external memory */

    httpd_config_t			http_cfg;
    int						sample_rate;
    int						bits;
    int						channels;
} streaming_http_audio_cfg_t;



#define STREAMING_HTTP_AUDIO_TASK_STACK          (3 * 1024)
#define STREAMING_HTTP_AUDIO_TASK_CORE           (1)
#define STREAMING_HTTP_AUDIO_TASK_PRIO           (23)
#define STREAMING_HTTP_AUDIO_RINGBUFFER_SIZE     (8 * 1024)

#define DEFAULT_STREAMING_HTTP_AUDIO_CONFIG() {\
    .out_rb_size        = STREAMING_HTTP_AUDIO_RINGBUFFER_SIZE,\
    .task_stack         = STREAMING_HTTP_AUDIO_TASK_STACK,\
    .task_core          = STREAMING_HTTP_AUDIO_TASK_CORE,\
    .task_prio          = STREAMING_HTTP_AUDIO_TASK_PRIO,\
    .stack_in_ext       = true,\
	.sample_rate		= 8000, \
	.bits				= 16, \
	.channels			= 1, \
}

/**
 * @brief      Create a handle to an Audio Element to encode incoming data using WAV format
 *
 * @param      config  The configuration
 *
 * @return     The audio element handle
 */
audio_element_handle_t streaming_http_audio_init(streaming_http_audio_cfg_t *config);


#ifdef __cplusplus
}
#endif

#endif
