#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include "mongoose/mongoose.h"
#include <functional>
#include <string>
#include <map>
#include <cstring>
#include <cstdint>

#undef poll

/**
 * A simple HTTP client using the Mongoose library.
 * Supports GET and POST requests with custom headers and timeouts.
 * Connection is closed after each request.
 * Call poll() periodically to process events.
 */
class HttpClient {
public:
    // Response object
    struct Response {
        int status = 0;
        std::map<std::string, std::string> headers;
        std::string body;
    };
    using Callback = std::function<void(const Response&)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    // Headers used in every request
    std::vector<std::string> headers;


    HttpClient() {
        mg_log_set(MG_LL_NONE);
        mg_mgr_init(&mgr);
    }

    ~HttpClient() {
        mg_mgr_free(&mgr);
    }

    void poll(int wait_time_ms = 0) {
        mg_mgr_poll(&mgr, wait_time_ms);
    }

    // Basic GET
    void get(const char* url,
             Callback onDone,
             ErrorCallback onError = nullptr,
             int timeout_ms = 5000)
    {
        request("GET", url, "", "", onDone, onError, timeout_ms);
    }

    // GET with headers
    void get(const char* url,
             const char* headers,
             Callback onDone,
             ErrorCallback onError = nullptr,
             int timeout_ms = 5000)
    {
        request("GET", url, "", headers, onDone, onError, timeout_ms);
    }

    // Basic POST
    void post(const char* url,
              const char* body,
              Callback onDone,
              ErrorCallback onError = nullptr,
              int timeout_ms = 5000)
    {
        request("POST", url, body, "", onDone, onError, timeout_ms);
    }

    // POST with headers
    void post(const char* url,
              const char* body,
              const char* headers,
              Callback onDone,
              ErrorCallback onError = nullptr,
              int timeout_ms = 5000)
    {
        request("POST", url, body, headers, onDone, onError, timeout_ms);
    }

    // JSON POST convenience
    void postJson(const char* url,
                  const char* json,
                  Callback onDone,
                  ErrorCallback onError = nullptr,
                  int timeout_ms = 5000)
    {
        post(url, json, "Content-Type: application/json", onDone, onError, timeout_ms);
    }

    
    /**
     * Sends an HTTP request with the specified parameters.
     *
     * @param method      The HTTP method to use (e.g., "GET", "POST").
     * @param url         The URL to which the request is sent.
     * @param data        The data to send with the request (for POST/PUT methods), or nullptr if not applicable.
     * @param headers     Additional HTTP headers to include in the request separated by \r\n (e.g., "Content-Type: application/json\r\nAccept: application/json"), or nullptr if none.
     * @param onDone      Callback function to be called upon successful completion of the request.
     * @param onError     Callback function to be called if an error occurs during the request.
     * @param timeout_ms  Timeout for the request in milliseconds.
     *
     * If the connection cannot be established, the onError callback is invoked with an error message.
     */
    void request(const char* method, const char* url, const char* data, const char* headers, Callback onDone, ErrorCallback onError, int timeout_ms)
    {
        // Own all strings inside the context to avoid dangling pointers
        auto* ctx = new RequestContext{};
        ctx->url     = url     ? url     : "";
        ctx->method  = method  ? method  : "GET";
        // Combine global headers and per-request headers
        ctx->headers.clear();
        for (const auto& h : this->headers) {
            ctx->headers += h;
            ctx->headers += "\r\n";
        }
        if (headers && *headers) {
            ctx->headers += headers;
            ctx->headers += "\r\n";
        }
        ctx->data    = data    ? data    : "";
        ctx->onDone  = std::move(onDone);
        ctx->onError = std::move(onError);
        ctx->timeout_ms = timeout_ms;

        struct mg_connection* c = mg_http_connect(&mgr, ctx->url.c_str(), ev_handler, ctx);
        if (!c) {
            if (ctx->onError) ctx->onError("Failed to connect");
            delete ctx;
        }
    }

private:
    struct RequestContext {
        std::string url;
        std::string method;
        std::string headers;
        std::string data;
        Callback onDone;
        ErrorCallback onError;
        int timeout_ms = 0;
    };

    mg_mgr mgr;

    static void ev_handler(struct mg_connection* c, int ev, void* ev_data) {
        RequestContext* ctx = (RequestContext*)c->fn_data;

        // Connection created
        if (ev == MG_EV_OPEN) {
            // Connection created. Store connect expiration time in c->data
            *(uint64_t *) c->data = mg_millis() + ctx->timeout_ms;

        // Every frame
        } else if (ev == MG_EV_POLL) {
            // Check for timeout until response is received or connection is closed
            if (mg_millis() > *(uint64_t *)c->data && !c->is_closing) {
                mg_error(c, "Timeout");
            }

        // TCP connection established
        } else if (ev == MG_EV_CONNECT) {
            // Connected to server. Extract host name from URL
            struct mg_str host = mg_url_host(ctx->url.c_str());
            const char* uri = mg_url_uri(ctx->url.c_str());
            const size_t body_len = ctx->data.size();

            if (c->is_tls) {
                struct mg_tls_opts opts = {};
                opts.name = host;
                mg_tls_init(c, &opts);
            }

            std::string requestStr;
            requestStr.reserve(256 + ctx->headers.size() + body_len);

            requestStr += ctx->method;
            requestStr += " ";
            requestStr += uri;
            requestStr += " HTTP/1.1\r\n";

            requestStr += "Host: ";
            requestStr.append(host.buf, host.len);
            requestStr += "\r\n";

            if (!ctx->headers.empty()) {
                requestStr += ctx->headers;
            }

            requestStr += "Content-Length: ";
            requestStr += std::to_string(body_len);
            requestStr += "\r\n\r\n";

            if (body_len > 0) {
                requestStr.append(ctx->data.data(), body_len);
            }

            mg_send(c, requestStr.data(), requestStr.size());
        }

        // TLS handshake complete – no-op
        else if (ev == MG_EV_TLS_HS) 
        {}
        
        // HTTP response received
        else if (ev == MG_EV_HTTP_MSG) {
            auto* hm = (struct mg_http_message*)ev_data;

            Response res;
            res.status = mg_http_status(hm);
            res.body.assign(hm->body.buf, hm->body.len);

            // parse headers
            for (int i = 0; i < MG_MAX_HTTP_HEADERS && hm->headers[i].name.len > 0; i++) {
                res.headers[std::string(hm->headers[i].name.buf, hm->headers[i].name.len)] =
                    std::string(hm->headers[i].value.buf, hm->headers[i].value.len);
            }

            if (ctx && ctx->onDone) ctx->onDone(res);
            c->is_closing = 1;
        }

        else if (ev == MG_EV_ERROR) {
            if (ctx && ctx->onError) ctx->onError((char*)ev_data);
            c->is_closing = 1;
        }

        else if (ev == MG_EV_CLOSE) {
            delete ctx;
        }
    }
};


#endif