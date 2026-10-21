#ifndef WEBSOCKET_CLIENT_H_
#define WEBSOCKET_CLIENT_H_

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <variant>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

// One WebSocket connection (ws:// or wss://). Messages are binary frames.
//
// It sends a text `ping` every 15 s and closes itself when nothing arrives for 35 s, so a
// half-open connection is noticed even when the server vanished without closing it. The server
// is expected to answer `pong`; text frames are consumed here and never reach OnMessage.
//
// A client is good for one connection. To reconnect, create a new one. Every method must be
// called on the io_context thread, and every callback runs there.
class WebSocketClient : public std::enable_shared_from_this<WebSocketClient> {
  public:
    using OnOpenFunc = std::function<void()>;
    using OnMessageFunc = std::function<void(const std::string &data)>;
    // Fires once. `code` is the close code from the server, or the code passed to Close(), or 0
    // when the connection failed or dropped without a close frame. `reason` says what happened.
    using OnCloseFunc = std::function<void(int code, const std::string &reason)>;

    static std::shared_ptr<WebSocketClient> Create(boost::asio::io_context &ioc);

    explicit WebSocketClient(boost::asio::io_context &ioc);
    ~WebSocketClient();

    void OnOpen(OnOpenFunc func) { on_open_fn_ = std::move(func); }
    void OnMessage(OnMessageFunc func) { on_message_fn_ = std::move(func); }
    void OnClose(OnCloseFunc func) { on_close_fn_ = std::move(func); }

    // Connects to ws://host[:port]/path?query or wss://host[:port]/path?query.
    void Open(const std::string &url);
    // Queues a binary frame. Frames sent before the connection opens are dropped.
    void Send(std::string data);
    // Sends the queued frames, then a close frame. OnClose fires with this code.
    void Close(int code = 1000, const std::string &reason = "");
    bool is_open() const { return open_ && !closing_ && !finished_; }

  private:
    using PlainStream = boost::beast::websocket::stream<boost::beast::tcp_stream>;
    using TlsStream =
        boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;

    boost::asio::io_context &ioc_;
    boost::asio::ssl::context ssl_ctx_;
    std::variant<std::monostate, PlainStream, TlsStream> ws_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::asio::steady_timer ping_timer_;
    boost::asio::steady_timer silence_timer_;
    boost::beast::flat_buffer buffer_;
    struct Frame {
        std::string data;
        bool text;
    };
    std::deque<Frame> write_queue_;

    std::string host_;
    std::string host_header_;
    std::string target_;
    uint16_t port_ = 0;
    bool open_ = false;
    bool writing_ = false;
    bool closing_ = false;
    bool finished_ = false;
    int close_code_ = 1000;
    std::string close_reason_;

    OnOpenFunc on_open_fn_;
    OnMessageFunc on_message_fn_;
    OnCloseFunc on_close_fn_;

    void OnResolve(boost::beast::error_code ec,
                   boost::asio::ip::tcp::resolver::results_type results);
    void OnConnect(boost::beast::error_code ec);
    void Handshake();
    void OnHandshake(boost::beast::error_code ec);
    void Read();
    void Queue(std::string data, bool text);
    void DoWrite();
    void DoClose();
    void SchedulePing();
    void ResetSilenceTimer();
    void Finish(int code, const std::string &reason);
};

#endif // WEBSOCKET_CLIENT_H_
