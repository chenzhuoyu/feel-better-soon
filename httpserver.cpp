#include "httpserver.h"

struct MethodName {
    std::string_view name;
    HttpMethod       method;
};

static const MethodName Methods[] = {
    { "GET"    , HttpMethod::GET    },
    { "PUT"    , HttpMethod::PUT    },
    { "POST"   , HttpMethod::POST   },
    { "PATCH"  , HttpMethod::PATCH  },
    { "DELETE" , HttpMethod::DELETE },
};

HttpServer::HttpServer(uint16_t port) : _srv(port) {
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

void HttpServer::route(const char *method, const char *path, Handler &&handler) {

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
    const char * path         = nullptr;
    const char * method       = nullptr;
    int          subver       = 0;
    size_t       path_len     = 0;
    size_t       method_len   = 0;
    size_t       header_count = sizeof(_headers) / sizeof(_headers[0]);

    /* check for buffer size */
    if (_read_len >= sizeof(_buffer)) {
        respond("HTTP/1.1 413 Payload Too Large\r\nContent-Length: 18\r\n\r\npayload too large\n");
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
        respond("HTTP/1.1 400 Bad Request\r\nContent-Length: 12\r\n\r\nparse error\n");
        return;
    }

    /* check for unknown errors */
    if (_header_len <= 0) {
        respond("HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\ninternal server error\n");
        return;
    }

    /* HTTP/1.1 only */
    if (subver != 1) {
        respond("HTTP/1.1 400 Bad Request\r\nContent-Length: 16\r\n\r\ninvalid version\n");
        return;
    }

    /* various flags */
    int    pos     = -1;
    bool   valid   = false;
    char * end_ptr = nullptr;

    /* update request fields */
    _req.body = "";
    _req.path = std::string_view(path, path_len);
    _req.headers.clear();

    /* parse the method */
    for (const auto &v : Methods) {
        if (v.name.size() == method_len && !strncmp(v.name.data(), method, method_len)) {
            valid = true;
            _req.method = v.method;
            break;
        }
    }

    /* check for methods */
    if (!valid) {
        respond("HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 19\r\n\r\nmethod not allowed\n");
        return;
    }

    /* build headers */
    for (int i = 0; i < header_count; i++) {
        _req.headers.emplace_back(HttpHeader {
            name  : std::string_view(_headers[i].name, _headers[i].name_len),
            value : std::string_view(_headers[i].value, _headers[i].value_len),
        });
    }

    /* find the "content-length" header */
    for (int i = 0; i < header_count; i++) {
        if (!strncasecmp(_headers[i].name, "content-length", _headers[i].name_len)) {
            pos = i;
            break;
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
        respond("HTTP/1.1 400 Bad Request\r\nContent-Length: 23\r\n\r\ninvalid Content-Length\n");
        return;
    }

    /* check for payload size */
    if (_header_len + body_len > sizeof(_buffer)) {
        respond("HTTP/1.1 413 Payload Too Large\r\nContent-Length: 18\r\n\r\npayload too large\n");
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
    printf("request body is %d bytes long\n", _req.body.size());
    printf("method is %d\n", _req.method);
    printf("path is %.*s\n", _req.path.size(), _req.path.data());
    printf("headers:\n");
    for (int i = 0; i != _req.headers.size(); i++) {
        printf("%.*s: %.*s\n", _req.headers[i].name.size(), _req.headers[i].name.data(),
            _req.headers[i].value.size(), _req.headers[i].value.data());
    }
    printf("body: %.*s\n", _req.body.size(), _req.body.data());
    respond("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nhello, world\n");
}
