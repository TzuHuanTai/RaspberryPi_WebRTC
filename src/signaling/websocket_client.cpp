#include "signaling/websocket_client.h"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <type_traits>

#include "common/logging.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

namespace {

constexpr int kConnectTimeoutSec = 10;
constexpr int kPingIntervalSec = 15;
// Two missed pings and a margin. The server answers every `ping` with `pong`.
constexpr int kSilenceTimeoutSec = 35;

struct WsUrl {
    bool use_tls = false;
    std::string host;
    uint16_t port = 0;
    std::string target = "/";
};

// Splits a ws/wss url. The port falls back to the scheme default, and IPv6 literals keep their
// brackets out of `host` so it can be handed to a resolver as-is.
bool ParseWsUrl(const std::string &url, WsUrl &out) {
    auto scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return false;
    }

    std::string scheme = url.substr(0, scheme_end);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
    if (scheme == "ws") {
        out.use_tls = false;
        out.port = 80;
    } else if (scheme == "wss") {
        out.use_tls = true;
        out.port = 443;
    } else {
        return false;
    }

    std::string rest = url.substr(scheme_end + 3);
    auto path_start = rest.find_first_of("/?");
    std::string authority = rest.substr(0, path_start);
    out.target = path_start == std::string::npos ? "/" : rest.substr(path_start);
    if (out.target.front() == '?') {
        out.target = "/" + out.target;
    }

    try {
        if (!authority.empty() && authority.front() == '[') { // IPv6 literal
            auto bracket_end = authority.find(']');
            if (bracket_end == std::string::npos) {
                return false;
            }
            out.host = authority.substr(1, bracket_end - 1);
            if (bracket_end + 1 < authority.size() && authority[bracket_end + 1] == ':') {
                out.port = static_cast<uint16_t>(std::stoi(authority.substr(bracket_end + 2)));
            }
        } else {
            auto colon = authority.find(':');
            out.host = authority.substr(0, colon);
            if (colon != std::string::npos) {
                out.port = static_cast<uint16_t>(std::stoi(authority.substr(colon + 1)));
            }
        }
    } catch (const std::exception &) {
        return false;
    }

    return !out.host.empty();
}

} // namespace

std::shared_ptr<WebSocketClient> WebSocketClient::Create(net::io_context &ioc) {
    return std::make_shared<WebSocketClient>(ioc);
}

WebSocketClient::WebSocketClient(net::io_context &ioc)
    : ioc_(ioc),
      ssl_ctx_(ssl::context::tls_client),
      resolver_(ioc),
      ping_timer_(ioc),
      silence_timer_(ioc) {}

WebSocketClient::~WebSocketClient() {
    ping_timer_.cancel();
    silence_timer_.cancel();
}

void WebSocketClient::Open(const std::string &url) {
    WsUrl parsed;
    if (!ParseWsUrl(url, parsed)) {
        // Report on the next turn, so OnClose never runs inside Open().
        net::post(ioc_, [self = shared_from_this()]() {
            self->Finish(0, "the url is not a ws:// or wss:// url");
        });
        return;
    }

    host_ = parsed.host;
    port_ = parsed.port;
    target_ = parsed.target;
    host_header_ = host_.find(':') != std::string::npos ? "[" + host_ + "]" : host_;
    if (!(port_ == 80 && !parsed.use_tls) && !(port_ == 443 && parsed.use_tls)) {
        host_header_ += ":" + std::to_string(port_);
    }

    if (parsed.use_tls) {
        // The SSL context comes from the BoringSSL inside libwebrtc.a. It is not a drop-in
        // replacement for OpenSSL, so keep to the basic calls. Configure it before the stream
        // exists: SSL_new copies the verify settings.
        ssl_ctx_.set_default_verify_paths();
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
        ws_.emplace<TlsStream>(ioc_, ssl_ctx_);
    } else {
        ws_.emplace<PlainStream>(ioc_);
    }

    resolver_.async_resolve(
        host_, std::to_string(port_),
        [self = shared_from_this()](beast::error_code ec, tcp::resolver::results_type results) {
            self->OnResolve(ec, results);
        });
}

void WebSocketClient::OnResolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (finished_) {
        return;
    }
    if (ec) {
        Finish(0, "could not resolve " + host_ + ": " + ec.message());
        return;
    }

    auto connect = [this, &results](auto &ws) {
        auto &tcp_stream = beast::get_lowest_layer(ws);
        tcp_stream.expires_after(std::chrono::seconds(kConnectTimeoutSec));
        tcp_stream.async_connect(results,
                                 [self = shared_from_this()](beast::error_code ec, tcp::endpoint) {
                                     self->OnConnect(ec);
                                 });
    };
    if (auto *ws = std::get_if<PlainStream>(&ws_)) {
        connect(*ws);
    } else if (auto *ws = std::get_if<TlsStream>(&ws_)) {
        connect(*ws);
    }
}

void WebSocketClient::OnConnect(beast::error_code ec) {
    if (finished_) {
        return;
    }
    if (ec) {
        Finish(0, "could not connect to " + host_header_ + ": " + ec.message());
        return;
    }

    auto *tls = std::get_if<TlsStream>(&ws_);
    if (!tls) {
        Handshake();
        return;
    }

    auto &ssl_stream = tls->next_layer();
    if (!SSL_set_tlsext_host_name(ssl_stream.native_handle(), host_.c_str())) {
        Finish(0, "could not set the TLS server name");
        return;
    }
    // Check that the certificate is for this host, not only that it is valid.
    ssl_stream.set_verify_callback(ssl::host_name_verification(host_));
    ssl_stream.async_handshake(
        ssl::stream_base::client, [self = shared_from_this()](beast::error_code ec) {
            if (self->finished_) {
                return;
            }
            if (ec) {
                self->Finish(0, "TLS handshake with " + self->host_ + " failed: " + ec.message());
                return;
            }
            self->Handshake();
        });
}

void WebSocketClient::Handshake() {
    auto handshake = [this](auto &ws) {
        // The WebSocket stream has its own timeouts from here on.
        beast::get_lowest_layer(ws).expires_never();
        ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));
        ws.set_option(websocket::stream_base::decorator([](websocket::request_type &req) {
            req.set(beast::http::field::user_agent, "pi-webrtc");
        }));
        ws.async_handshake(host_header_, target_,
                           [self = shared_from_this()](beast::error_code ec) {
                               self->OnHandshake(ec);
                           });
    };
    if (auto *ws = std::get_if<PlainStream>(&ws_)) {
        handshake(*ws);
    } else if (auto *ws = std::get_if<TlsStream>(&ws_)) {
        handshake(*ws);
    }
}

void WebSocketClient::OnHandshake(beast::error_code ec) {
    if (finished_) {
        return;
    }
    if (ec) {
        // A rejected upgrade (e.g. 401 for a bad token) also ends up here.
        Finish(0, "WebSocket handshake with " + host_header_ + " failed: " + ec.message());
        return;
    }

    open_ = true;
    SchedulePing();
    ResetSilenceTimer();
    Read();
    if (on_open_fn_) {
        on_open_fn_();
    }
    DoWrite();
}

void WebSocketClient::Read() {
    auto read = [this](auto &ws) {
        ws.async_read(buffer_, [self = shared_from_this(), &ws](beast::error_code ec, std::size_t) {
            if (self->finished_) {
                return;
            }
            if (ec) {
                if (ec == websocket::error::closed) {
                    int code = 0;
                    std::string reason;
                    std::visit(
                        [&code, &reason](auto &ws) {
                            if constexpr (!std::is_same_v<std::decay_t<decltype(ws)>,
                                                          std::monostate>) {
                                code = ws.reason().code;
                                reason = std::string(ws.reason().reason.c_str());
                            }
                        },
                        self->ws_);
                    self->Finish(code, reason.empty() ? "closed by the server" : reason);
                } else if (self->closing_) {
                    self->Finish(self->close_code_, self->close_reason_);
                } else {
                    self->Finish(0, "connection lost: " + ec.message());
                }
                return;
            }

            std::string data = beast::buffers_to_string(self->buffer_.data());
            self->buffer_.consume(self->buffer_.size());
            self->ResetSilenceTimer();
            if (!ws.got_text() && self->on_message_fn_) {
                self->on_message_fn_(data);
            }
            if (!self->finished_) {
                self->Read();
            }
        });
    };
    if (auto *ws = std::get_if<PlainStream>(&ws_)) {
        read(*ws);
    } else if (auto *ws = std::get_if<TlsStream>(&ws_)) {
        read(*ws);
    }
}

void WebSocketClient::Send(std::string data) { Queue(std::move(data), false); }

void WebSocketClient::Queue(std::string data, bool text) {
    if (finished_ || closing_) {
        return;
    }
    if (!open_) {
        DEBUG_PRINT("WebSocket is not open yet, dropping a frame.");
        return;
    }
    write_queue_.push_back({std::move(data), text});
    DoWrite();
}

void WebSocketClient::DoWrite() {
    if (writing_ || finished_ || !open_) {
        return;
    }
    if (write_queue_.empty()) {
        if (closing_) {
            DoClose();
        }
        return;
    }

    writing_ = true;
    auto write = [this](auto &ws) {
        ws.text(write_queue_.front().text);
        ws.async_write(net::buffer(write_queue_.front().data),
                       [self = shared_from_this()](beast::error_code ec, std::size_t) {
                           self->writing_ = false;
                           if (self->finished_) {
                               self->write_queue_.clear(); // Finish() kept the frame being written
                               return;
                           }
                           if (ec) {
                               self->Finish(0, "could not send: " + ec.message());
                               return;
                           }
                           self->write_queue_.pop_front();
                           self->DoWrite();
                       });
    };
    if (auto *ws = std::get_if<PlainStream>(&ws_)) {
        write(*ws);
    } else if (auto *ws = std::get_if<TlsStream>(&ws_)) {
        write(*ws);
    }
}

void WebSocketClient::Close(int code, const std::string &reason) {
    if (finished_ || closing_) {
        return;
    }
    closing_ = true;
    close_code_ = code;
    close_reason_ = reason;
    if (!open_) {
        Finish(code, reason.empty() ? "closed before it opened" : reason);
        return;
    }
    // A close frame may not overlap a write, so it goes out after the queue drains.
    DoWrite();
}

void WebSocketClient::DoClose() {
    websocket::close_reason reason(static_cast<websocket::close_code>(close_code_), close_reason_);
    auto close = [this, &reason](auto &ws) {
        ws.async_close(reason, [self = shared_from_this()](beast::error_code) {
            self->Finish(self->close_code_, self->close_reason_);
        });
    };
    if (auto *ws = std::get_if<PlainStream>(&ws_)) {
        close(*ws);
    } else if (auto *ws = std::get_if<TlsStream>(&ws_)) {
        close(*ws);
    }
}

void WebSocketClient::SchedulePing() {
    ping_timer_.expires_after(std::chrono::seconds(kPingIntervalSec));
    ping_timer_.async_wait([weak_self = weak_from_this()](const boost::system::error_code &ec) {
        auto self = weak_self.lock();
        if (ec || !self || self->finished_ || self->closing_) {
            return;
        }
        self->Queue("ping", true);
        self->SchedulePing();
    });
}

void WebSocketClient::ResetSilenceTimer() {
    silence_timer_.expires_after(std::chrono::seconds(kSilenceTimeoutSec));
    silence_timer_.async_wait([weak_self = weak_from_this()](const boost::system::error_code &ec) {
        auto self = weak_self.lock();
        if (ec || !self || self->finished_) {
            return;
        }
        self->Finish(0, "no pong for " + std::to_string(kSilenceTimeoutSec) + " seconds");
    });
}

void WebSocketClient::Finish(int code, const std::string &reason) {
    if (finished_) {
        return;
    }
    finished_ = true;
    open_ = false;

    ping_timer_.cancel();
    silence_timer_.cancel();
    resolver_.cancel();
    // A write in progress still reads the first frame; its handler clears it.
    if (writing_) {
        write_queue_.erase(std::next(write_queue_.begin()), write_queue_.end());
    } else {
        write_queue_.clear();
    }

    // Pending reads and writes complete with an error and see finished_.
    beast::error_code ignored;
    if (auto *ws = std::get_if<PlainStream>(&ws_)) {
        beast::get_lowest_layer(*ws).socket().close(ignored);
    } else if (auto *ws = std::get_if<TlsStream>(&ws_)) {
        beast::get_lowest_layer(*ws).socket().close(ignored);
    }

    on_open_fn_ = nullptr;
    on_message_fn_ = nullptr;
    if (auto on_close = std::move(on_close_fn_)) {
        on_close_fn_ = nullptr;
        on_close(code, reason);
    }
}
