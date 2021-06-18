idf_component_register(SRCS "main.cpp" "wifi.c" "webserver.c" "wav_create.c" "streaming_wav.c" "streaming_server.c"
							"streaming_http_audio.c"
                    INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../webserver_files FLASH_IN_PROJECT)
                    