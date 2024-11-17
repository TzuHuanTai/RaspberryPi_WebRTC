#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <iostream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

void handle_request(beast::tcp_stream &stream) {
    try {
        beast::flat_buffer buffer;

        // read the request packet
        http::request<http::string_body> req;
        http::read(stream, buffer, req);

        std::cout << "Request received:\n" << req << std::endl;

        // responsed content
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "Boost.Beast");
        res.set(http::field::content_type, "text/plain");
        res.body() = "Hello, World!";
        res.prepare_payload();

        // response
        http::write(stream, res);
    } catch (const beast::system_error &e) {
        if (e.code() != beast::errc::not_connected) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

int main() {
    try {
        asio::io_context ioc;

        tcp::acceptor acceptor{ioc, {asio::ip::address_v6::any(), 8080}};
        std::cout << "Server is running on http://*:8080\n";

        while (true) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);

            beast::tcp_stream stream{std::move(socket)};
            handle_request(stream);

            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_send, ec);

            if (ec && ec != beast::errc::not_connected) {
                throw beast::system_error{ec};
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
