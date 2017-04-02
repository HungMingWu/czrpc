/************************************************
 * 监听多个端口的rpc服务端
************************************************/
#include <iostream>
#include "czrpc/server/server.hpp"
#include "common.pb.h"

void echo(const czrpc::message::request_ptr& req, const czrpc::message::response_ptr& rsp)
{
    req->message()->PrintDebugString();
    rsp->set_response(req->message());
}

int main()
{
    // 1.创建rpc服务器对象
    czrpc::server::server server;
    try
    {
        // 2.绑定echo函数
        server.bind("echo", &echo);

        // 3.监听多个端口并启动事件循环（非阻塞）
        // 服务端默认启动一个ios线程和一个work线程
        std::vector<czrpc::base::endpoint> ep;
        ep.emplace_back(czrpc::base::endpoint{ "127.0.0.1", 50051 });
        ep.emplace_back(czrpc::base::endpoint{ "127.0.0.1", 50052 });
        server.listen(ep).run();
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }

    std::cin.get();
    return 0;
}
