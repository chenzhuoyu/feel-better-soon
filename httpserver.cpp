#include "progmem.h"
#include "httpserver.h"

struct MethodName {
    char       name[8];
    HttpMethod method;
};

static const MethodName Methods[] PROGMEM = {
    { "GET"    , HttpMethod::GET    },
    { "PUT"    , HttpMethod::PUT    },
    { "POST"   , HttpMethod::POST   },
    { "PATCH"  , HttpMethod::PATCH  },
    { "DELETE" , HttpMethod::DELETE },
};

static const char *HTTP_400_BAD_REQUEST PROGMEM =
    "HTTP/1.1 400 Bad Request\r\n"
    "Content-Length: 12\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "bad request\n";

static const char *HTTP_404_NOT_FOUND PROGMEM =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Length: 10\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "not found\n";

static const char *HTTP_405_METHOD_NOT_ALLOWED PROGMEM =
    "HTTP/1.1 405 Method Not Allowed\r\n"
    "Content-Length: 19\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "method not allowed\n";

static const char *HTTP_413_PAYLOAD_TOO_LARGE PROGMEM =
    "HTTP/1.1 413 Payload Too Large\r\n"
    "Content-Length: 18\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "payload too large\n";

static const char *HTTP_500_INTERNAL_SERVER_ERROR PROGMEM =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Length: 22\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "internal server error\n";

static const char *HTTP_501_NOT_IMPLEMENTED PROGMEM =
    "HTTP/1.1 501 Not Implemented\r\n"
    "Content-Length: 16\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "not implemented\n";

HttpServer::HttpServer(uint16_t port, const HttpRoutingTable *routes) : _srv(port), _routes(routes) {
    _req.headers.reserve(sizeof(_headers) / sizeof(_headers[0]));
}

void HttpServer::poll() {
    auto conn = _srv.available();
    auto state = conn.connected();

    /* handle new connections (one at a time) */
    if (state && accept(std::move(conn))) {
        _state = State::ReadHeaders;
    }

    /* main state machine */
    switch (_state) {
        case State::Idle          : break;
        case State::Finished      : state_finished(); break;
        case State::ReadHeaders   : state_read_headers(); break;
        case State::ReadPayload   : state_read_payload(); break;
        case State::WriteResponse : state_write_response(); break;
        case State::HandleRequest : state_handle_request(); break;
    }
}

void HttpServer::begin() {
    _srv.setNoDelay(true);
    _srv.begin();
}

void HttpServer::close() {
    WiFiClient::stopAll();
    _srv.close();
}

bool HttpServer::accept(WiFiClient conn) {
    if (_state != State::Idle) {
        conn.stop();
        return false;
    } else {
        _conn = std::move(conn);
        _conn.keepAlive(10, 3, 5);
        return true;
    }
}

void HttpServer::respond(HttpResponse &&resp) {
    if (_state != State::WriteResponse) {
        _resp = std::move(resp);
        _state = State::WriteResponse;
    }
}

void HttpServer::state_finished() {
    _conn.stop();
    _resp = nullptr;
    _state = State::Idle;
    _read_len = 0;
}

void HttpServer::state_read_headers() {
    bool         ok           = false;
    int          pos          = -1;
    const char * path         = nullptr;
    const char * delim        = nullptr;
    const char * method       = nullptr;
    int          subver       = 0;
    char *       end_ptr      = nullptr;
    size_t       path_len     = 0;
    size_t       method_len   = 0;
    size_t       header_count = sizeof(_headers) / sizeof(_headers[0]);

    /* check for buffer size */
    if (_read_len >= sizeof(_buffer)) {
        respond(HTTP_413_PAYLOAD_TOO_LARGE);
        return;
    }

    /* read the remaining bytes */
    auto rem = sizeof(_buffer) - _read_len;
    auto ret = _conn.read(&_buffer[_read_len], rem);

    /* check for read size */
    if (ret == 0) {
        return;
    }

    /* update the read pointers */
    _last_len = _read_len;
    _read_len += ret;

    /* parse the request */
    _header_len = phr_parse_request(
        _buffer,
        _read_len,
        &method,
        &method_len,
        &path,
        &path_len,
        &subver,
        _headers,
        &header_count,
        _last_len
    );

    /* request incomplete */
    if (_header_len == -2) {
        return;
    }

    /* check for header errors */
    if (_header_len == -1) {
        respond(HTTP_400_BAD_REQUEST);
        return;
    }

    /* check for unknown errors */
    if (_header_len <= 0) {
        respond(HTTP_500_INTERNAL_SERVER_ERROR);
        return;
    }

    /* HTTP/1.1 only */
    if (subver != 1) {
        respond(HTTP_400_BAD_REQUEST);
        return;
    }

    /* clear body and header buffer */
    _req.body = "";
    _req.headers.clear();

    /* split the query string */
    if ((delim = strchr(path, '?')) == nullptr) {
        _req.path  = std::string_view(path, path_len);
        _req.query = "";
    } else {
        _req.path  = std::string_view(path, delim - path);
        _req.query = std::string_view(delim + 1, path_len - (delim - path) - 1);
    }

    /* parse the method */
    for (const auto &v : Methods) {
        if (!strncmp_P(method, v.name, method_len) && !pgm_read_byte(&v.name[method_len])) {
            ok = true;
            _req.method = static_cast<HttpMethod>(pgm_read_byte(&v.method));
            break;
        }
    }

    /* check for methods */
    if (!ok) {
        respond(HTTP_405_METHOD_NOT_ALLOWED);
        return;
    }

    /* build headers */
    for (int i = 0; i < header_count; i++) {
        auto name = _headers[i].name;
        auto nlen = _headers[i].name_len;

        /* add to header buffer */
        _req.headers.emplace_back(HttpHeader {
            name  : std::string_view(name, nlen),
            value : std::string_view(_headers[i].value, _headers[i].value_len),
        });

        /* find the "Content-Length" header */
        if (!strncasecmp(name, "content-length", nlen)) {
            if (pos == -1) {
                pos = i;
                continue;
            } else {
                respond(HTTP_400_BAD_REQUEST);
                return;
            }
        }

        /* "Transfer-Encoding" is not supported */
        if (!strncasecmp(name, "transfer-encoding", nlen)) {
            respond(HTTP_501_NOT_IMPLEMENTED);
            return;
        }
    }

    /* no content length */
    if (pos == -1) {
        _state = State::HandleRequest;
        return;
    }

    /* parse the content-length */
    auto body     = &_buffer[_header_len];
    auto body_len = strtoul(_headers[pos].value, &end_ptr, 10);

    /* check for errors */
    if (end_ptr - _headers[pos].value != _headers[pos].value_len) {
        respond(HTTP_400_BAD_REQUEST);
        return;
    }

    /* check for payload size */
    if (_header_len + body_len > sizeof(_buffer)) {
        respond(HTTP_413_PAYLOAD_TOO_LARGE);
        return;
    }

    /* set the body if any */
    if (body_len != 0) {
        _req.body = std::string_view(body, body_len);
    }

    /* check for body length */
    if (_read_len < body_len + header_count) {
        _state = State::ReadPayload;
    } else {
        _state = State::HandleRequest;
    }
}

void HttpServer::state_read_payload() {
    size_t req = _header_len + _req.body.size();
    size_t rem = req - _read_len;

    /* read body bytes if needed */
    if (rem != 0) {
        _read_len += _conn.read(&_buffer[_read_len], rem);
    }

    /* check for required size */
    if (_read_len >= req) {
        _state = State::HandleRequest;
    }
}

void HttpServer::state_write_response() {
    size_t nb;
    size_t rem = _resp.len;

    /* send the response if any */
    if (rem != 0) {
        if ((nb = _conn.write(_resp.buf, rem)) != 0) {
            _resp.buf += nb;
            _resp.len -= nb;
        }
    }

    /* no more data remains */
    if (_resp.len == 0) {
        _state = State::Finished;
    }
}

void HttpServer::state_handle_request() {
    bool mx   = false;
    auto rt   = _routes;
    auto path = _req.path.data();
    auto size = _req.path.size();

    /* find the handler */
    for (;;) {
        auto v0 = pgm_read_byte(rt->path);
        auto mt = pgm_typed_byte(&rt->method);

        /* not found */
        if (v0 == 0) {
            break;
        }

        /* compare request path */
        if (strncmp_P(path, rt->path, size) || pgm_read_byte(&rt->path[size])) {
            rt++;
            continue;
        }

        /* found the handler */
        if (mt == _req.method) {
            respond(pgm_typed_ptr(&rt->handler)(_req));
            return;
        }

        /* move to next entry */
        rt++;
        mx = true;
    }

    /* check if path exists */
    if (!mx) {
        respond(HTTP_404_NOT_FOUND);
    } else {
        respond(HTTP_405_METHOD_NOT_ALLOWED);
    }
}
