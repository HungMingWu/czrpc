#pragma once

#include "base/thread_pool.hpp"
#include "client_base.hpp"
#include "sub_router.hpp"

namespace czrpc
{
namespace client
{
class sub_client : public client_base
{
public:
    sub_client(const sub_client&) = delete;
    sub_client& operator=(const sub_client&) = delete;
    sub_client() : heartbeats_work_(heartbeats_ios_), heartbeats_timer_(heartbeats_ios_)
    {
        client_type_ = client_type::sub_client;
    }

    virtual ~sub_client()
    {
        stop();
    }

    virtual void run() override final
    {
        static const std::size_t thread_num = 1;
        threadpool_.init_thread_num(thread_num);
        client_base::run();
        sync_connect();
        start_heartbeats_thread();
    }

    virtual void stop() override final
    {
        stop_heartbeats_thread();
        client_base::stop();
    }

    template<typename Function>
    void subscribe(const std::string& topic_name, const Function& func)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
        sub_router::singleton::get()->bind(topic_name, func);
    }

    template<typename Function, typename Self>
    void subscribe(const std::string& topic_name, const Function& func, Self* self)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
        sub_router::singleton::get()->bind(topic_name, func, self);
    }

    template<typename Function>
    void subscribe_raw(const std::string& topic_name, const Function& func)
    {
        sync_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
        sub_router::singleton::get()->bind_raw(topic_name, func);
    }

    template<typename Function, typename Self>
    void subscribe_raw(const std::string& topic_name, const Function& func, Self* self)
    {
        sync_connect();
        client_flag flag{ serialize_mode::non_serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
        sub_router::singleton::get()->bind_raw(topic_name, func, self);
    }

    void cancel_subscribe(const std::string& topic_name)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", cancel_subscribe_topic_flag });
        sub_router::singleton::get()->unbind(topic_name);
    }

    void cancel_subscribe_raw(const std::string& topic_name)
    {
        sync_connect();
        client_flag flag{ serialize_mode::serialize, client_type_ };
        async_write(request_content{ 0, flag, topic_name, "", cancel_subscribe_topic_flag });
        sub_router::singleton::get()->unbind_raw(topic_name);
    }

    bool is_subscribe(const std::string& topic_name)
    {
        return sub_router::singleton::get()->is_bind(topic_name);
    }

    bool is_subscribe_raw(const std::string& topic_name)
    {
        return sub_router::singleton::get()->is_bind_raw(topic_name);
    }

private:
    void async_read_head()
    {
        boost::asio::async_read(get_socket(), boost::asio::buffer(push_head_buf_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            if (!get_socket().is_open())
            {
                std::cout << "Socket is not open" << std::endl;
                return;
            }

            if (ec)
            {
                std::cout << ec.message() << std::endl;
                return;
            }

            if (async_check_head())
            {
                async_read_content();
            }
            else
            {
                async_read_head();
            }
        });
    }

    bool async_check_head()
    {
        memcpy(&push_head_, push_head_buf_, sizeof(push_head_buf_));
        if (push_head_.protocol_len + push_head_.message_name_len + push_head_.body_len > max_buffer_len)
        {
            std::cout << "Content len is too big" << std::endl;
            return false;
        }
        return true;
    }

    void async_read_content()
    {
        content_.clear();
        content_.resize(sizeof(serialize_mode) + push_head_.protocol_len + push_head_.message_name_len + push_head_.body_len);
        boost::asio::async_read(get_socket(), boost::asio::buffer(content_), 
                                [this](boost::system::error_code ec, std::size_t)
        {
            async_read_head();

            if (!get_socket().is_open())
            {
                std::cout << "Socket is not open" << std::endl;
                return;
            }

            if (ec)
            {
                std::cout << ec.message() << std::endl;
                return;
            }

            threadpool_.add_task(&sub_client::router_thread, this, make_content());
            last_active_time_ = time(nullptr);
        });
    }

    push_content make_content()
    {
        push_content content;
        memcpy(&content.mode, &content_[0], sizeof(content.mode));
        content.protocol.assign(&content_[sizeof(content.mode)], push_head_.protocol_len);
        content.message_name.assign(&content_[sizeof(content.mode) + push_head_.protocol_len], push_head_.message_name_len);
        content.body.assign(&content_[sizeof(content.mode) + push_head_.protocol_len + push_head_.message_name_len], push_head_.body_len);
        return std::move(content);
    }

    void heartbeats_timer()
    {
        if ((time(nullptr) - last_active_time_) * 1000 > heartbeats_milli)
        {
            try
            {
                sync_connect();
                client_flag flag{ serialize_mode::serialize, client_type_ };
                async_write(request_content{ 0, flag, heartbeats_flag, "", heartbeats_flag });
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << std::endl;
            }
        }
    }

    void retry_subscribe()
    {
        try
        {
            for (auto& topic_name : sub_router::singleton::get()->get_all_topic())
            {
                client_flag flag{ serialize_mode::serialize, client_type_ };
                async_write(request_content{ 0, flag, topic_name, "", subscribe_topic_flag });
            }
        }
        catch (std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    void start_heartbeats_thread()
    {
        heatbeats_thread_ = std::make_unique<std::thread>([this]{ heartbeats_ios_.run(); });
        heartbeats_timer_.bind([this]{ heartbeats_timer(); });
        heartbeats_timer_.start(heartbeats_milli);
    }

    void stop_heartbeats_thread()
    {
        heartbeats_ios_.stop();
        if (heatbeats_thread_ != nullptr)
        {
            if (heatbeats_thread_->joinable())
            {
                heatbeats_thread_->join();
            }
        }
    }

    void sync_connect()
    {
        if (try_connect())
        {
            async_read_head();
            retry_subscribe();
        }
    }

    void router_thread(const push_content& content)
    {
        bool ok = false;
        if (content.mode == serialize_mode::serialize)
        {
            message_ptr req = serialize_util::singleton::get()->deserialize(content.message_name, content.body);
            ok = sub_router::singleton::get()->route(content.protocol, req);
        }
        else if (content.mode == serialize_mode::non_serialize)
        {
            ok = sub_router::singleton::get()->route_raw(content.protocol, content.body);
        }
        if (!ok)
        {
            std::cout << "Route failed" << std::endl;
        }
    }

private:
    char push_head_buf_[push_header_len];
    push_header push_head_;
    std::vector<char> content_;

    boost::asio::io_service heartbeats_ios_;
    boost::asio::io_service::work heartbeats_work_;
    std::unique_ptr<std::thread> heatbeats_thread_;
    atimer<> heartbeats_timer_;
    thread_pool threadpool_;
    time_t last_active_time_ = 0;
};

}
}

