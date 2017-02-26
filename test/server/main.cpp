#include <iostream>
#include "czrpc/server/server.hpp"
#include "proto_message.pb.h"

using namespace czrpc::base;

std::shared_ptr<google::protobuf::Message> request_person_info(const std::shared_ptr<google::protobuf::Message>& in_message, const std::string& session_id)
{
    std::cout << "session id: " << session_id << std::endl;
    auto message = std::dynamic_pointer_cast<request_person_info_message>(in_message);
    message->PrintDebugString();
#if 1
    auto out_message = std::make_shared<response_person_info_message>();
    out_message->set_name("Tom");
    out_message->set_age(21);
#else
    auto out_message = std::make_shared<response_error>();
    out_message->set_error_code(100);
    out_message->set_error_string("Not found person info");
#endif
    return out_message;
}

class test
{
public:
    std::string echo(const std::string& str)
    {
        return str;
    }
};

void client_connect_notify(const std::string& session_id)
{
    log_info("connect session id: {}", session_id);
}

void client_disconnect_notify(const std::string& session_id)
{
    log_info("disconnect session id: {}", session_id);
}

int main()
{
    czrpc::server::server server;
    test t;
    try
    {
        server.set_client_connect_notify(std::bind(&client_connect_notify, std::placeholders::_1));
        server.set_client_disconnect_nofity(std::bind(&client_disconnect_notify, std::placeholders::_1));
        server.bind("request_person_info", &request_person_info);
        server.bind_raw("echo", &test::echo, &t);

        std::vector<czrpc::base::endpoint> ep;
        ep.emplace_back(czrpc::base::endpoint{ "127.0.0.1", 50051 });
        ep.emplace_back(czrpc::base::endpoint{ "127.0.0.1", 50052 });
        server.listen(ep).ios_threads(std::thread::hardware_concurrency()).work_threads(10).run();
    }
    catch (std::exception& e)
    {
        log_warn(e.what());
        return 0;
    }

    std::cin.get();
    return 0;
}
