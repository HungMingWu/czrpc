#pragma once

#include "client_base.hpp"

namespace czrpc
{
namespace client
{
class rpc_client : public client_base
{
public:
    rpc_client(const rpc_client&) = delete;
    rpc_client& operator=(const rpc_client&) = delete;
    rpc_client()
    {
        client_type_ = client_type::rpc_client;
        init_timer();
    }

    virtual ~rpc_client()
    {
        stop();
    }

    virtual void run() override final
    {
        client_base::run();
        try_connect();
    }

    virtual void stop() override final
    {
        timer_.destroy();
        client_base::stop();
    }

    message_ptr call(const std::string& func_name, const message_ptr& message)
    {
        serialize_util::singleton::get()->check_message(message);
        try_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        auto rsp = write_and_read(request_content{ 0, flag, func_name, message->GetDescriptor()->full_name(), 
                                  serialize_util::singleton::get()->serialize(message) });
        return serialize_util::singleton::get()->deserialize(rsp.message_name, rsp.body);
    }

    std::string call_raw(const std::string& func_name, const std::string& body)
    {
        try_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        auto rsp = write_and_read(request_content{ 0, flag, func_name, "", body });
        return std::move(rsp.body);
    }

private:
    response_content write_and_read(const request_content& content)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        write(content);
        timer_.start(timeout_milli_);
        auto guard = make_guard([this]{ timer_.stop(); });
        return std::move(read());
    }

    void init_timer()
    {
        timer_.bind([this]{ disconnect(); });
        timer_.set_single_shot(true);
    }

private:
    atimer<> timer_;
    std::mutex mutex_;
};

}
}

