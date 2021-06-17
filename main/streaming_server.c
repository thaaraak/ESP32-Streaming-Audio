/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "streaming_server.h"
#include "wav_create.h"
#include "streaming_wav.h"

#define HTTPD_SCRATCH_BUF  MAX(HTTPD_MAX_REQ_HDR_LEN, HTTPD_MAX_URI_LEN)

// This private structure comes from esp_http_priv.h

struct httpd_req_aux {
    struct sock_db *sd;                             /*!< Pointer to socket database */
    char            scratch[HTTPD_SCRATCH_BUF + 1]; /*!< Temporary buffer for our operations (1 byte extra for null termination) */
    size_t          remaining_len;                  /*!< Amount of data remaining to be fetched */
    char           *status;                         /*!< HTTP response's status code */
    char           *content_type;                   /*!< HTTP response's content type */
    bool            first_chunk_sent;               /*!< Used to indicate if first chunk sent */
    unsigned        req_hdrs_count;                 /*!< Count of total headers in request packet */
    unsigned        resp_hdrs_count;                /*!< Count of additional headers in response packet */
    struct resp_hdr {
        const char *field;
        const char *value;
    } *resp_hdrs;                                   /*!< Additional headers in response packet */
    struct http_parser_url url_parse_res;           /*!< URL parsing result, used for retrieving URL elements */
#ifdef CONFIG_HTTPD_WS_SUPPORT
    bool ws_handshake_detect;                       /*!< WebSocket handshake detection flag */
    httpd_ws_type_t ws_type;                        /*!< WebSocket frame type */
    bool ws_final;                                  /*!< WebSocket FIN bit (final frame or not) */
#endif
};


/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

#define SCRATCH_BUFSIZE  8192

static const char *TAG = "streaming-server";

struct streaming_server_data {

    char scratch[SCRATCH_BUFSIZE];
    void (*command_callback)( const char*, char* );
};

struct streaming_server_data *streaming_server_data = NULL;

static esp_err_t stream_handler_old(httpd_req_t *req)
{
    ESP_LOGE(TAG, "In Stream Handler" );
	httpd_resp_set_type(req, "audio/x-wav");

	int16_t* data;
	int total = create_wav( 1, 1000, &data );
	char* buf = (char*) data;

	while ( total > 0 ) {

	    size_t chunksize = total > SCRATCH_BUFSIZE ? SCRATCH_BUFSIZE : total;
        ESP_LOGE(TAG, "Sending: %d bytes out of %d remaining", chunksize, total);

        if (httpd_resp_send_chunk(req, buf, chunksize) != ESP_OK) {
            ESP_LOGE(TAG, "File sending failed!");
            httpd_resp_sendstr_chunk(req, NULL);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            free(data);
            return ESP_FAIL;
        }

        total -= chunksize;
        buf += chunksize;
	}

    free(data);

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}


static esp_err_t stream_handler(httpd_req_t *req)
{
    ESP_LOGE(TAG, "In Stream Handler" );
	httpd_resp_set_type(req, "audio/x-wav");

    //struct httpd_req_aux *ra = req->aux;
    //const char   *hdr_ptr = ra->scratch;

    streaming_wav_t	wav;

    ESP_LOGE(TAG, "Sending Header" );
	streaming_wav_init( &wav, SCRATCH_BUFSIZE );
    httpd_resp_send_chunk(req, (const char*)&(wav.hdr), sizeof(wav.hdr));

    size_t chunksize = SCRATCH_BUFSIZE;

    for ( ;; ) {

    	streaming_wav_play( &wav );

        if (httpd_resp_send_chunk(req, (const char*)wav.buf, chunksize) != ESP_OK) {
            ESP_LOGE(TAG, "File sending failed!");
            httpd_resp_sendstr_chunk(req, NULL);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            streaming_wav_destroy( &wav );
            return ESP_FAIL;
        }

	}

    streaming_wav_destroy( &wav );
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}


esp_err_t start_streaming_server()
{
    if (streaming_server_data) {
        ESP_LOGE(TAG, "Streaming server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    streaming_server_data = calloc(1, sizeof(struct streaming_server_data));
    if (!streaming_server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = 8080;
    config.ctrl_port = 8081;

    httpd_uri_t stream = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        /* Let's pass response string in user
         * context to demonstrate it's usage */
        .user_ctx  = "Stream Handler"
    };

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &stream);
        ESP_LOGI(TAG, "Completed Registering URI handlers");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return ESP_FAIL;
}






