#include "signal.h"

#include <unistd.h>

#include <future>
#include <iostream>
#include <sstream>
#include <string>

#include "signalrclient/hub_connection.h"
#include "signalrclient/hub_connection_builder.h"
#include "signalrclient/signalr_client_config.h"
#include "signalrclient/signalr_value.h"

SignalServer::SignalServer(std::string server_url) {
    //connection_ = std::unique_ptr<signalr::hub_connection>(
    //    signalr::hub_connection_builder::create(server_url).build());
}

SignalServer::~SignalServer() {
    std::promise<void> stop_task;
    connection_->stop([&stop_task](std::exception_ptr exception) { stop_task.set_value(); });

    stop_task.get_future().get();
}

void SignalServer::Start() {
    std::promise<void> start_task;
    connection_->start([&start_task](std::exception_ptr exception) { start_task.set_value(); });
    start_task.get_future().get();
};

void SignalServer::Send(std::string method, const char* context) {
    std::promise<void> send_task;
    std::vector<signalr::value> args{context};
    connection_->invoke(method, args,
                        [&send_task](const signalr::value& value, std::exception_ptr exception) {
                            send_task.set_value();
                        });
    send_task.get_future().get();
};

void send_message(signalr::hub_connection& connection, const std::string& message)
{
    std::vector<signalr::value> args { std::string("c++"), message };

    // if you get an internal compiler error uncomment the lambda below or install VS Update 4
    connection.invoke("Send", args, [](const signalr::value& value, std::exception_ptr exception)
    {
        try
        {
            if (exception)
            {
                std::rethrow_exception(exception);
            }

            if (value.is_string())
            {
                std::cout << "Received: " << value.as_string() << std::endl;
            }
            else
            {
                std::cout << "hub method invocation has completed" << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cout << "Error while sending data: " << e.what() << std::endl;
        }
    });
}

void chat(const std::string server_url) {

    signalr::hub_connection connection =
        signalr::hub_connection_builder::create(server_url).build();

    connection.on("ReceiveEcho", [](const std::vector<signalr::value>& m) {
        std::cout << "ReceiveEcho: " <<m[0].as_string() << std::endl;
    });

    std::promise<void> start_task;
    connection.start([&start_task](std::exception_ptr exception) {
        start_task.set_value();
    });
    start_task.get_future().get();

    // std::promise<void> task;
    // connection.start([&connection, &task](std::exception_ptr exception)
    // {
    //     if (exception)
    //     {
    //         try
    //         {
    //             std::rethrow_exception(exception);
    //         }
    //         catch (const std::exception & ex)
    //         {
    //             std::cout << "exception when starting connection: " << ex.what() << std::endl;
    //         }
    //         task.set_value();
    //         return;
    //     }

    //     std::cout << "Enter your message:";
    //     while (connection.get_connection_state() == signalr::connection_state::connected)
    //     {
    //         std::string message;
    //         std::getline(std::cin, message);

    //         if (message == ":q" || connection.get_connection_state() != signalr::connection_state::connected)
    //         {
    //             break;
    //         }

    //         send_message(connection, message);
    //     }

    //     connection.stop([&task](std::exception_ptr exception)
    //     {
    //         try
    //         {
    //             if (exception)
    //             {
    //                 std::rethrow_exception(exception);
    //             }

    //             std::cout << "connection stopped successfully" << std::endl;
    //         }
    //         catch (const std::exception & e)
    //         {
    //             std::cout << "exception when stopping connection: " << e.what() << std::endl;
    //         }

    //         task.set_value();
    //     });
    // });

    // task.get_future().get();


    std::promise<void> send_task;
    std::vector<signalr::value> args{"Hello world"};
    connection.invoke("Echo", args,
                      [&send_task](const signalr::value& value, std::exception_ptr exception) {
                          send_task.set_value();
                      });

    connection.invoke("Echo", args,
                      [&send_task](const signalr::value& value, std::exception_ptr exception) {
                          send_task.set_value();
                      });
    send_task.get_future().get();
    sleep(10);

    std::promise<void> stop_task;
    connection.stop([&stop_task](std::exception_ptr exception) { stop_task.set_value(); });
    stop_task.get_future().get();
}

int main() {
    std::string server_url = "http://192.168.187.36:5000/SignalingServer";

    chat(server_url);

    sleep(10);

    return 0;
}
