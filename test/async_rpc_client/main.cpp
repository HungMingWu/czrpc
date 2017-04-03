#include <iostream>
#include "czrpc/client/client.hpp"
#include "proto_message.pb.h"

#define IS_SAME(message, other_message) (message->GetDescriptor()->full_name() == other_message::descriptor()->full_name())

czrpc::client::async_rpc_client client;

void test_func()
{
    while (true)
    {
        try
        {
            auto message = std::make_shared<request_person_info_message>();
            message->set_name("Jack");
            message->set_age(20);

            client.async_call("request_person_info", message).result([](const message_ptr& in_message, const czrpc::base::error_code& ec)
            {
                if (ec)
                {
                    std::cout << ec.message() << std::endl;
                    return;
                }

                if (IS_SAME(in_message, response_person_info_message))
                {
                    auto message = std::dynamic_pointer_cast<response_person_info_message>(in_message); 
                    message->PrintDebugString();
                }
                else if (IS_SAME(in_message, response_error))
                {
                    auto message = std::dynamic_pointer_cast<response_error>(in_message); 
                    message->PrintDebugString();
                }
            });

            client.async_call_raw("echo", "Hello world").result([](const std::string& in_message, const czrpc::base::error_code& ec)
            {
                if (ec)
                {
                    std::cout << ec.message() << std::endl;
                    return;
                }
                std::cout << in_message << std::endl;
            });
        }
        catch (std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void connect_success_notify()
{
    std::cout << "connect success..." << std::endl;
}

int main()
{   
    try
    {
        client.set_connect_success_notify(std::bind(&connect_success_notify));
        client.connect({ "127.0.0.1", 50051 }).timeout(3000).run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

#if 0
    try
    {
        auto message = std::make_shared<request_person_info_message>();
        message->set_name("Jack");
        message->set_age(20);

        client.async_call("request_person_info", message).result([](const message_ptr& in_message, const czrpc::base::error_code& ec)
        {
            if (ec)
            {
                std::cout << ec.message() << std::endl;
                return;
            }

            if (IS_SAME(in_message, response_person_info_message))
            {
                auto message = std::dynamic_pointer_cast<response_person_info_message>(in_message); 
                message->PrintDebugString();
            }
            else if (IS_SAME(in_message, response_error))
            {
                auto message = std::dynamic_pointer_cast<response_error>(in_message); 
                message->PrintDebugString();
            }
        });
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }
    std::cin.get();

#else
    std::thread t(test_func);
    std::thread t2(test_func);

    t.join();
    t2.join();
#endif

    return 0;
}
