#include <functional>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include <signalrclient/hub_connection.h>
#include <signalrclient/hub_connection_builder.h>
#include <signalrclient/signalr_value.h>

#include "config.h"
#include "signal.h"

SignalingConfig config;

SignalServer::SignalServer(std::string url)
    : url(url),
      connection_(signalr::hub_connection_builder::create(url).build())
{
}

void SignalServer::Connect()
{
    std::promise<void> start_task;
    connection_.start([&start_task](std::exception_ptr exception)
                      { start_task.set_value(); });
    start_task.get_future().get();
};

void SignalServer::Disconnect()
{
    std::promise<void> stop_task;
    connection_.stop([&stop_task](std::exception_ptr exception)
                     { stop_task.set_value(); });
    stop_task.get_future().get();
};

void SignalServer::Subscribe(std::string event_name, const signalr::hub_connection::method_invoked_handler &handler)
{
    connection_.on(event_name, handler);
};

void SignalServer::SendMessage(std::string method, const char *context)
{
    std::promise<void> send_task;
    std::vector<signalr::value> args{std::string("c++"), context};
    connection_.invoke(method, args,
                       [&send_task](const signalr::value &value, std::exception_ptr exception)
                       {
                           send_task.set_value();
                       });
    send_task.get_future().get();
};

void send_message(signalr::hub_connection &connection, const std::string &message)
{
    std::vector<signalr::value> args{std::string("c++"), message};

    // if you get an internal compiler error uncomment the lambda below or install VS Update 4
    connection.invoke("Echo", args, [](const signalr::value &value, std::exception_ptr exception)
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
        } });
}

void chat(signalr::hub_connection &connection)
{
    connection.on("Echo", [](const std::vector<signalr::value> &m)
                  { std::cout << std::endl
                              << m[0].as_string() << std::endl
                              << "Enter your message: "; });

    std::promise<void> task;
    connection.start([&connection, &task](std::exception_ptr exception)
                     {
        if (exception)
        {
            try
            {
                std::rethrow_exception(exception);
            }
            catch (const std::exception & ex)
            {
                std::cout << "exception when starting connection: " << ex.what() << std::endl;
            }
            task.set_value();
            return;
        }

        std::cout << "Enter your message:";
        while (connection.get_connection_state() == signalr::connection_state::connected)
        {
            std::string message;
            std::getline(std::cin, message);

            if (message == ":q" || connection.get_connection_state() != signalr::connection_state::connected)
            {
                break;
            }

            send_message(connection, message);
        }

        connection.stop([&task](std::exception_ptr exception)
        {
            try
            {
                if (exception)
                {
                    std::rethrow_exception(exception);
                }

                std::cout << "connection stopped successfully" << std::endl;
            }
            catch (const std::exception & e)
            {
                std::cout << "exception when stopping connection: " << e.what() << std::endl;
            }

            task.set_value();
        }); });

    send_message(connection, "connection finish");

    task.get_future().get();
}

void test(signalr::hub_connection &connection)
{
    connection.on("Echo", [](const std::vector<signalr::value> &m)
                  { std::cout << "Echo: " << m[0].as_string() << std::endl; });

    std::promise<void> start_task;
    connection.start([&start_task](std::exception_ptr exception)
                     { start_task.set_value(); });
    start_task.get_future().get();

    send_message(connection, "Hello world");

    std::cout << "test => stop signalr in 10 sec!" << std::endl;
    sleep(10);

    std::promise<void> stop_task;
    connection.stop([&stop_task](std::exception_ptr exception)
                    { stop_task.set_value(); });
    stop_task.get_future().get();
}

void Echo(const std::vector<signalr::value> &m)
{
    std::cout << "Echo: " << m[0].as_string() << std::endl;
}

int main()
{
    auto signalr = new SignalServer(config.signaling_url);

    signalr->Subscribe("Echo", Echo);

    signalr->Connect();

    signalr->SendMessage("Echo", "Hello world");
    sleep(5);

    signalr->SendMessage("Echo", "This is Richard");
    sleep(5);

    signalr->Disconnect();

    std::cout << "=> main: end" << std::endl;

    return 0;
}
