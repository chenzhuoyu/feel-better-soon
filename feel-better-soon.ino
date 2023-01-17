#include <ESP8266WiFi.h>
#include "httpserver.h"

#define AP_SSID     "Oxygen-IoT"
#define AP_PASSWD   "MagicBox-2016"

#define UART_BAUD   2000000
#define SERVER_PORT 9999

static HttpResponse http_query_led(const HttpRequest &req);

static const char StatusTab[][16] PROGMEM = {
    "IDLE",
    "NO_SSID_AVAIL",
    "SCAN_COMPLETED",
    "CONNECTED",
    "CONNECT_FAILED",
    "CONNECTION_LOST",
    "WRONG_PASSWORD",
    "DISCONNECTED",
};

static const HttpRoutingTable HttpRoutes[] PROGMEM = {
    { HttpMethod::GET, "/led", http_query_led },
    {},
};

static uint32_t    _blink  = 0;
static HttpServer  _server = HttpServer(SERVER_PORT, HttpRoutes);
static wl_status_t _status = WL_IDLE_STATUS;

static HttpResponse http_query_led(const HttpRequest &req) {
    printf("request body is %d bytes long\n", req.body.size());
    printf("method is %d\n", req.method);
    printf("path is %.*s\n", req.path.size(), req.path.data());
    printf("query is %.*s\n", req.query.size(), req.query.data());
    printf("headers:\n");
    for (int i = 0; i != req.headers.size(); i++) {
        printf("%.*s: %.*s\n", req.headers[i].name.size(), req.headers[i].name.data(),
            req.headers[i].value.size(), req.headers[i].value.data());
    }
    printf("body: %.*s\n", req.body.size(), req.body.data());
    return "HTTP/1.1 200 OK\r\n\r\n";
}

static void on_status_changed(wl_status_t status) {
    switch (status) {
        case WL_CONNECTED    : _server.begin(); Serial.println("Server started."); break;
        case WL_DISCONNECTED : _server.close(); Serial.println("Server stopped."); break;
    }
}

static void blink_poll() {
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_BUILTIN, (_blink = 0));
    } else {
        digitalWrite(LED_BUILTIN, (++_blink & 0x8000) >> 15);
    }
}

static void status_poll() {
    if (WiFi.status() != _status) {
        Serial.printf("Status Changed: %s => %s\n", StatusTab[_status], StatusTab[WiFi.status()]);
        on_status_changed((_status = WiFi.status()));
    }
}

static void server_poll() {
    if (WiFi.status() == WL_CONNECTED) {
        _server.poll();
    }
}

void setup() {
    delay(1000);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    /* initialize serial port */
    Serial.begin(UART_BAUD);
    Serial.println();
    Serial.println("Device is starting ...");
    Serial.flush();

    /* initialize the LCD screen */
    // st7789_init();
    // st7789_clear_screen(0);

    /* connect to Wi-Fi access point */
    WiFi.begin(AP_SSID, AP_PASSWD);
    WiFi.setAutoReconnect(true);
}

void loop() {
    blink_poll();
    status_poll();
    server_poll();
    // int x = rand() % ST7789_WIDTH;
    // int y = rand() % ST7789_HEIGHT;
    // uint16_t color = (uint16_t)(rand());
    // st7789_set_window(x, y, 1, 1);
    // st7789_draw_pixel(color);
}