#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <functional>
#include <string_view>
#include <ESP8266WiFi.h>

#include "picohttpparser.h"

struct HttpHeader {
    std::string_view name;
    std::string_view value;
};

enum class HttpMethod : uint8_t {
    GET,
    PUT,
    POST,
    PATCH,
    DELETE,
};

struct HttpRequest {
    HttpMethod              method;
    std::string_view        path;
    std::string_view        body;
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
    static HttpResponse create(const char *buf)             { return create(buf, slen(buf)); }
    static HttpResponse create(const char *buf, size_t len) { return HttpResponse(buf, len, true); }

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
    char       _buffer[4096] = {};
    phr_header _headers[128] = {};

private:
    HttpRequest  _req        = {};
    HttpResponse _resp       = nullptr;
    State        _state      = State::Idle;
    size_t       _last_len   = 0;
    size_t       _read_len   = 0;
    size_t       _header_len = 0;

public:
    using Handler = std::function<std::string_view(HttpRequest &)>;
    explicit HttpServer(uint16_t port);

public:
    void poll();
    void begin();
    void close();
    void route(const char *method, const char *path, Handler &&handler);

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