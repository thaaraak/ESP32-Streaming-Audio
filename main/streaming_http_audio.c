// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#include "streaming_http_audio.h"

#include "esp_log.h"
#include "audio_mem.h"
#include "audio_element.h"
#include "wav_header.h"
#include "audio_error.h"

static const char *TAG = "streaming_http_audio";


typedef struct streaming_http_audio {

    bool			active;
    httpd_req_t*	req;
    int				buf_size;
    char*			buf;

    int				sample_rate;
    int				bits;
    int				channels;

} streaming_http_audio_t;

static esp_err_t _streaming_http_audio_destroy(audio_element_handle_t self)
{
    streaming_http_audio_t *sha = (streaming_http_audio_t *)audio_element_getdata(self);
    audio_free(sha);
    return ESP_OK;
}
static esp_err_t _streaming_http_audio_open(audio_element_handle_t self)
{
    ESP_LOGD(TAG, "_streaming_http_audio_open");
    return ESP_OK;
}

static esp_err_t _streaming_http_audio_close(audio_element_handle_t self)
{
    ESP_LOGD(TAG, "_streaming_http_audio_close");
    if (AEL_STATE_PAUSED != audio_element_get_state(self)) {
        audio_element_set_byte_pos(self, 0);
        audio_element_set_total_bytes(self, 0);
    }
    return ESP_OK;
}

static int _streaming_http_audio_process(audio_element_handle_t self, char *in_buffer, int in_len)
{
    int r_size = audio_element_input(self, in_buffer, in_len);

    int out_len = r_size;
    if (r_size > 0) {

    	out_len = audio_element_output(self, in_buffer, r_size);
        if (out_len > 0) {
            audio_element_update_byte_pos(self, out_len);
        }
    }

    return out_len;
}

int cnt = 0;

static int _streaming_http_audio_write(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{

    streaming_http_audio_t *sha = (streaming_http_audio_t *)audio_element_getdata(self);

    // If there is no active stream then just simply return len
    // This effectively ignores the audio block

    if ( !sha->active )
    	return len;

    /*
    if ( cnt++ % 100 == 0 )
    	ESP_LOGI(TAG, "In Audio Write Length: %d", len );
	*/

    // Transform incoming buffer of size "len" from stereo 16 bit
    // to mono 16 bit WAV format

    int16_t* src = (int16_t*)buffer;
    int16_t* dest = (int16_t*)sha->buf;

    for ( int i = 0 ; i < len/2 ; i += 2 )
     	dest[i/2] = src[i];

    httpd_req_t *req = sha->req;

    if (httpd_resp_send_chunk(req, (const char*)sha->buf, len/2 ) != ESP_OK) {
         ESP_LOGE(TAG, "Streaming send failed");
         httpd_resp_sendstr_chunk(req, NULL);
         sha->active = false;
         return ESP_FAIL;
    }

     return len;
}

void _streaming_wav_header( wav_header_t* w, streaming_http_audio_t* sha )
{
	// Simple hack here for an endless stream is to set the len to maximum value.
	// Both Chrome and Brave seem to have no problems with this.

	int len = 0xFFFFFFFF;

	w->riff.chunk_id = 0X46464952;			// "RIFF"
	w->riff.format = 0X45564157;			// "WAVE"
	w->riff.chunk_size = len;

	w->fmt.chunk_id = 0X20746D66;			// "fmt "
	w->fmt.audio_format = 1;
	w->fmt.bits_per_sample = sha->bits;
	w->fmt.block_align = sha->channels * sha->bits/8;
	w->fmt.byterate = sha->sample_rate * sha->channels * sha->bits/8;
	w->fmt.chunk_size = 16;
	w->fmt.num_of_channels = sha->channels;
	w->fmt.samplerate = sha->sample_rate;

	w->data.chunk_id = 0X61746164;
	w->data.chunk_size = len;
}


static esp_err_t _stream_handler(httpd_req_t *req)
{
    ESP_LOGE(TAG, "In Stream Handler" );
	httpd_resp_set_type(req, "audio/x-wav");

	wav_header_t wav;
    streaming_http_audio_t *sha = (streaming_http_audio_t *)audio_element_getdata( (audio_element_handle_t) req->user_ctx);

    ESP_LOGE(TAG, "Sending Header" );
	_streaming_wav_header( &wav, sha );
    httpd_resp_send_chunk(req, (const char*)&(wav), sizeof(wav));

    ESP_LOGE(TAG, "Header Sent" );

    sha->req = req;
    sha->active = true;

    // This seems hokey. This function can't return while the stream is active
    // because the socket will be closed (Assumption) so need to keep looping
    // here until _streaming_http_audio_write closes and sets active to false
    // Events with attached data might be a solution

    while ( sha->active )
        vTaskDelay(100 / portTICK_PERIOD_MS);

    return ESP_OK;
}



esp_err_t _start_streaming_server( audio_element_handle_t el, streaming_http_audio_cfg_t *config )
{
    httpd_handle_t server = NULL;

    httpd_uri_t stream = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = _stream_handler,
        .user_ctx  = el
    };

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config->http_cfg.uri_match_fn = httpd_uri_match_wildcard;
    config->http_cfg.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config->http_cfg.server_port);
    if (httpd_start(&server, &(config->http_cfg)) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &stream);
        ESP_LOGI(TAG, "Completed Registering URI handlers");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return ESP_FAIL;
}


audio_element_handle_t streaming_http_audio_init(streaming_http_audio_cfg_t *config)
{
    streaming_http_audio_t *sha = audio_calloc(1, sizeof(streaming_http_audio_t));
    AUDIO_MEM_CHECK(TAG, sha, {return NULL;});

    audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    cfg.destroy = _streaming_http_audio_destroy;
    cfg.process = _streaming_http_audio_process;
    cfg.open = _streaming_http_audio_open;
    cfg.close = _streaming_http_audio_close;
    cfg.task_stack = STREAMING_HTTP_AUDIO_TASK_STACK;

    cfg.buffer_len = 4096;

    if (config) {
        if (config->task_stack) {
            cfg.task_stack = config->task_stack;
        }
        cfg.stack_in_ext = config->stack_in_ext;
        cfg.task_prio = config->task_prio;
        cfg.task_core = config->task_core;
        cfg.out_rb_size = config->out_rb_size;
    }

    cfg.tag = "sha";
    cfg.write = _streaming_http_audio_write;

    // Only need half the buffer size on output as it is mono
    sha->buf_size = cfg.buffer_len/2;
    sha->buf = audio_malloc( sha->buf_size );
    sha->active = false;
    sha->sample_rate = config->sample_rate;
	sha->bits = config->bits;
	sha->channels = config->channels;

    ESP_LOGE(TAG, "Streaming Audio Config: Size: %d Sample Rate: %d Bits: %d Channels: %d",
    	    sha->buf_size,
    	    sha->sample_rate,
    		sha->bits,
    		sha->channels
    		);

    audio_element_handle_t el = audio_element_init(&cfg);
    AUDIO_MEM_CHECK(TAG, el, {audio_free(sha); return NULL;});
    audio_element_setdata(el, sha);

    _start_streaming_server( el, config );

    ESP_LOGD(TAG, "streaming_http_audio_init");
    return el;
}
