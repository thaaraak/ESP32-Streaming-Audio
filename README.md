ESP32-Streaming-Audio
=====================

This application uses the [Espressif IoT Development Framework](https://github.com/espressif/esp-idf) and the [Espressif Audio Development Framework](https://github.com/espressif/esp-adf) to create an outbound streaming http audio source.

The application consists of
* A web server on port 80 which streams the content held in the "webserver_files" project directory
* A new ESP-ADF audio pipeline element "streaming_http_audio.c" which listens to the I2S stream and contains
* A web server on port 8080 which is used to stream the audio

The html file "index3.html" contains the audio control which connects to the streaming web server

There are two "main" files
* main.c - which is the full I2S -> Streaming HTTP Audio pipeline. This also contains a web server which runs on port 80. To access go to (http://esp32-streaming/index3.html)
* main_simple.c - which is a simple streaming test using just the web server

SDKConfig needs to be edited to set the wifi SSID and password at:
```
CONFIG_ESP_WIFI_SSID="xx"
CONFIG_ESP_WIFI_PASSWORD="xx"
CONFIG_ESP_HOSTNAME="esp32-streaming"
```
