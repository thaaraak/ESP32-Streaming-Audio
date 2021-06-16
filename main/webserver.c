/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "webserver.h"
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

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define SCRATCH_BUFSIZE  8192

static const char *TAG = "web-server";

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];

    void (*command_callback)( const char*, char* );
};

struct file_server_data *server_data = NULL;

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".pdf")) {
        return httpd_resp_set_type(req, "application/pdf");
    } else if (IS_FILE_EXT(filename, ".html")) {
            return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".css")) {
            return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".svg")) {
            return httpd_resp_set_type(req, "image/svg+xml");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".wav")) {
        return httpd_resp_set_type(req, "audio/x-wav");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }

    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}


/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}



/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t stream_handler_old(httpd_req_t *req)
{
    ESP_LOGE(TAG, "In Stream Handler" );
	httpd_resp_set_type(req, "audio/x-wav");

    struct httpd_req_aux *ra = req->aux;
    const char   *hdr_ptr = ra->scratch;         /*!< Request headers are kept in scratch buffer */

    printf( "\n===== HEADERS =====\n");
    printf( hdr_ptr );
    printf( "\n===== END-HEADERS =====\n");

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

    struct httpd_req_aux *ra = req->aux;
    const char   *hdr_ptr = ra->scratch;         /*!< Request headers are kept in scratch buffer */

    streaming_wav_t	wav;

    printf( "\n===== HEADERS =====\n");
    printf( hdr_ptr );
    printf( "\n===== END-HEADERS =====\n");

    ESP_LOGE(TAG, "Sending Header" );
	streaming_wav_init( &wav, SCRATCH_BUFSIZE );
    httpd_resp_send_chunk(req, (const char*)&(wav.hdr), sizeof(wav.hdr));

    size_t chunksize = SCRATCH_BUFSIZE;

    int cnt = 0;

    for ( ;; ) {

    	streaming_wav_play( &wav );

        if (httpd_resp_send_chunk(req, (const char*)wav.buf, chunksize) != ESP_OK) {
            ESP_LOGE(TAG, "File sending failed!");
            httpd_resp_sendstr_chunk(req, NULL);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
            streaming_wav_destroy( &wav );
            free(wav.buf);
            return ESP_FAIL;
        }

	}

    streaming_wav_destroy( &wav );

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}




static esp_err_t command_handler(httpd_req_t *req)
{
    char* buf;
    char* response;
    size_t buf_len;

    response = malloc(256);

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            server_data->command_callback( buf, response );
        }
        free(buf);
    }

    httpd_resp_send(req, response, strlen(response));
    free( response );

    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/command", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/command URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    }

    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}


esp_err_t start_webserver(const char *base_path, void (*cb)( const char *, char * ))
{
    /* Validate file storage base path */
    if (!base_path || strcmp(base_path, "/spiffs") != 0) {
        ESP_LOGE(TAG, "File server presently supports only '/spiffs' as base path");
        return ESP_ERR_INVALID_ARG;
    }

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    }
    strlcpy(server_data->base_path, base_path,sizeof(server_data->base_path));
    server_data->command_callback = cb;

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t command = {
        .uri       = "/command",
        .method    = HTTP_GET,
        .handler   = command_handler,
        /* Let's pass response string in user
         * context to demonstrate it's usage */
        .user_ctx  = "Command Handler"
    };

    httpd_uri_t stream = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        /* Let's pass response string in user
         * context to demonstrate it's usage */
        .user_ctx  = "Stream Handler"
    };

    /* URI handler for getting uploaded files */
    httpd_uri_t file_download = {
        .uri       = "/*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = download_get_handler,
        .user_ctx  = server_data    // Pass server data as context
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
        httpd_register_uri_handler(server, &command);
        httpd_register_uri_handler(server, &stream);
        httpd_register_uri_handler(server, &file_download);
        ESP_LOGI(TAG, "Completed Registering URI handlers");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return ESP_FAIL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, 
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        start_webserver( server_data->base_path, server_data->command_callback );
    }
}




