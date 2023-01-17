#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <string_view>
#include <unordered_map>
#include <ESP8266WiFi.h>

#include "picohttpparser.h"

enum class HttpMethod : uint8_t {
    GET,
    PUT,
    POST,
    PATCH,
    DELETE,
};

struct HttpHeader {
    std::string_view name;
    std::string_view value;
};

struct HttpRequest {
    HttpMethod              method;
    std::string_view        path;
    std::string_view        body;
    std::string_view        query;
    std::vector<HttpHeader> headers;
};

struct HttpResponse {
    size_t       len   = 0;
    const char * buf   = nullptr;
    bool         owned = false;

private:
    HttpResponse(const char *buf, size_t len, bool owned) :
        len   (len),
        buf   (buf),
        owned (owned) {}

public:
    ~HttpResponse() {
        if (owned) {
            free(const_cast<char *>(buf));
        }
    }

public:
    HttpResponse(const HttpResponse &)            = delete;
    HttpResponse &operator=(const HttpResponse &) = delete;

public:
    HttpResponse(HttpResponse &&other)            { swap(other); }
    HttpResponse &operator=(HttpResponse &&other) { swap(other); return *this; }

public:
    HttpResponse(const char *buf)             : HttpResponse(buf, slen(buf)) {}
    HttpResponse(const char *buf, size_t len) : HttpResponse(buf, len, false) {}

public:
    static HttpResponse take(const char *buf)             { return take(buf, slen(buf)); }
    static HttpResponse take(const char *buf, size_t len) { return HttpResponse(buf, len, true); }

public:
    void swap(HttpResponse &other) {
        std::swap(len, other.len);
        std::swap(buf, other.buf);
        std::swap(owned, other.owned);
    }

private:
    static inline size_t slen(const char *buf) {
        if (buf == nullptr) {
            return 0;
        } else {
            return strlen(buf);
        }
    }
};

struct HttpRoutingTable {
    HttpMethod     method;
    char           path[256];
    HttpResponse (*handler)(const HttpRequest &);
};

class HttpServer {
    enum class State {
        Idle,
        Finished,
        ReadHeaders,
        ReadPayload,
        HandleRequest,
        WriteResponse,
    };

private:
    WiFiServer _srv;
    WiFiClient _conn;

private:
    State  _state      = State::Idle;
    size_t _last_len   = 0;
    size_t _read_len   = 0;
    size_t _header_len = 0;

private:
    char       _buffer[4096] = {};
    phr_header _headers[32]  = {};

private:
    HttpRequest              _req    = {};
    HttpResponse             _resp   = nullptr;
    const HttpRoutingTable * _routes = nullptr;

public:
    explicit HttpServer(uint16_t port, const HttpRoutingTable *routes);

public:
    void poll();
    void begin();
    void close();

private:
    bool accept(WiFiClient conn);
    void respond(HttpResponse &&resp);

private:
    void state_finished();
    void state_read_headers();
    void state_read_payload();
    void state_write_response();
    void state_handle_request();
};

#endif