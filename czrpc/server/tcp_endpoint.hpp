#pragma once

#include "io_service_pool.hpp"
#include "connection.hpp"

namespace czrpc
{
namespace server
{
class tcp_endpoint
{
public:
    tcp_endpoint(const router_callback& route_func, 
                 const handle_error_callback& remove_all_topic_func,
                 const std::function<void(const std::string&)>& client_connect,
                 const std::function<void(const std::string&)>& client_disconnect) 
        : acceptor_(io_service_pool::singleton::get()->get_io_service()),
        route_(route_func), 
        handle_error_(remove_all_topic_func),
        client_connect_notify_(client_connect),
        client_disconnect_notify_(client_disconnect){}

    void listen(const std::string& ip, unsigned short port)
    {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::address_v4::from_string(ip), port);
        acceptor_.open(ep.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(ep);
        acceptor_.listen();
    }

    void accept()
    {
        auto new_conn = std::make_shared<connection>(io_service_pool::singleton::get()->get_io_service(), 
                                                     route_, handle_error_, client_connect_notify_, client_disconnect_notify_);
        acceptor_.async_accept(new_conn->socket(), [this, new_conn](boost::system::error_code ec)
        {
            if (!ec)
            {
                new_conn->start();
            }
            accept();
        });
    }

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    router_callback route_;
    handle_error_callback handle_error_;

    std::function<void(const std::string&)> client_connect_notify_ = nullptr;
    std::function<void(const std::string&)> client_disconnect_notify_ = nullptr;
};

}
}

