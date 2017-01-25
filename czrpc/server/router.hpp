#pragma once

#include <iostream>
#include <unordered_map>
#include <map>
#include <tuple>
#include <type_traits>
#include "base/header.hpp"
#include "base/function_traits.hpp"
#include "base/thread_pool.hpp"
#include "base/logger.hpp"
#include "base/singleton.hpp"
#include "base/serialize_util.hpp"
#include "connection.hpp"

namespace czrpc
{
namespace server
{
class invoker_function
{
public:
    using function_t = std::function<void(const message_ptr&, message_ptr&)>;
    invoker_function() = default;
    invoker_function(const function_t& func) : func_(func) {}

    void operator()(const std::string& call_id, const std::string& message_name, 
                    const std::string& body, const connection_ptr& conn)
    {
        try
        {
            message_ptr out_message;
            func_(serialize_util::singleton::get()->deserialize(message_name, body), out_message);
            std::string in_message_name = out_message->GetDescriptor()->full_name();
            std::string in_body = serialize_util::singleton::get()->serialize(out_message);
            if (!in_message_name.empty() && !in_body.empty())
            {
                conn->async_write(response_content{ call_id, in_message_name, in_body });
            }
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
            conn->disconnect();
        }
    }

private:
    function_t func_ = nullptr;
};

class invoker_function_raw
{
public:
    using function_t = std::function<void(const std::string&, std::string&)>;
    invoker_function_raw() = default;
    invoker_function_raw(const function_t& func) : func_(func) {}

    void operator()(const std::string& call_id, const std::string& body, const connection_ptr& conn)
    {
        try
        {
            std::string out_body;
            func_(body, out_body);
            if (!out_body.empty())
            {
                conn->async_write(response_content{ call_id, "", out_body });
            }
        }
        catch (std::exception& e)
        {
            log_warn(e.what());
            conn->disconnect();
        }
    }

private:
    function_t func_ = nullptr;
};

class router
{
    DEFINE_SINGLETON(router);
public:
    router() = default;

    void multithreaded(std::size_t num)
    {
        threadpool_.init_thread_num(num);
    }

    void stop()
    {
        threadpool_.stop();
    }

    template<typename Function>
    void bind(const std::string& protocol, const Function& func)
    {
        bind_non_member_func(protocol, func);
    }

    template<typename Function, typename Self>
    void bind(const std::string& protocol, const Function& func, Self* self)
    {
        bind_member_func(protocol, func, self); 
    }

    void unbind(const std::string& protocol)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        invoker_map_.erase(protocol);
    }

    bool is_bind(const std::string& protocol)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        auto iter = invoker_map_.find(protocol);
        if (iter != invoker_map_.end())
        {
            return true;
        }
        return false;
    }

    template<typename Function>
    void bind_raw(const std::string& protocol, const Function& func)
    {
        bind_non_member_func_raw(protocol, func);
    }

    template<typename Function, typename Self>
    void bind_raw(const std::string& protocol, const Function& func, Self* self)
    {
        bind_member_func_raw(protocol, func, self); 
    }

    void unbind_raw(const std::string& protocol)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        invoker_raw_map_.erase(protocol);
    }

    bool is_bind_raw(const std::string& protocol)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        auto iter = invoker_raw_map_.find(protocol);
        if (iter != invoker_raw_map_.end())
        {
            return true;
        }
        return false;
    }

    bool route(const request_content& content, const client_flag& flag, const connection_ptr& conn)
    {
        if (flag.type == client_type::rpc_client)
        {
            if (flag.mode == serialize_mode::serialize)
            {
                std::lock_guard<std::mutex> lock(map_mutex_);
                auto iter = invoker_map_.find(content.protocol);
                if (iter == invoker_map_.end())
                {
                    return false;
                }
                threadpool_.add_task(iter->second, content.call_id, content.message_name, content.body, conn);
            }
            else if (flag.mode == serialize_mode::non_serialize)
            {
                std::lock_guard<std::mutex> lock(raw_map_mutex_);
                auto iter = invoker_raw_map_.find(content.protocol);
                if (iter == invoker_raw_map_.end())
                {
                    return false;
                }
                threadpool_.add_task(iter->second, content.call_id, content.body, conn);           
            }
            else
            {
                log_warn("Invaild serialize mode: {}", static_cast<unsigned int>(flag.mode));
                return false;
            }
        }
        else if (flag.type == client_type::pub_client)
        {
            threadpool_.add_task(pub_coming_helper_, content.protocol, content.body, flag.mode);
        }
        else if (flag.type == client_type::sub_client)
        {
            threadpool_.add_task(sub_coming_helper_, content.protocol, content.body, conn);
        }
        else
        {
            log_warn("Invaild client type: {}", static_cast<unsigned int>(flag.type));
            return false;
        }

        return true;
    }

private:
    template<typename Function>
    static typename std::enable_if<std::is_void<typename std::result_of<Function(const message_ptr&)>::type>::value>::type
    call(const Function& func, const message_ptr& in_message, message_ptr&)
    {
        func(in_message);
    }

    template<typename Function>
    static typename std::enable_if<!std::is_void<typename std::result_of<Function(const message_ptr&)>::type>::value>::type
    call(const Function& func, const message_ptr& in_message, message_ptr& out_message)
    {
        out_message = func(in_message);
    }

    template<typename Function, typename Self>
    static typename std::enable_if<std::is_void<typename std::result_of<Function(Self, const message_ptr&)>::type>::value>::type
    call_member(const Function& func, Self* self, const message_ptr& in_message, message_ptr&)
    {
        (*self.*func)(in_message);
    }

    template<typename Function, typename Self>
    static typename std::enable_if<!std::is_void<typename std::result_of<Function(Self, const message_ptr&)>::type>::value>::type
    call_member(const Function& func, Self* self, const message_ptr& in_message, message_ptr& out_message)
    {
        out_message = (*self.*func)(in_message);
    }

    template<typename Function>
    static typename std::enable_if<std::is_void<typename std::result_of<Function(const std::string&)>::type>::value>::type
    call_raw(const Function& func, const std::string& body, std::string&)
    {
        func(body);
    }

    template<typename Function>
    static typename std::enable_if<!std::is_void<typename std::result_of<Function(const std::string&)>::type>::value>::type
    call_raw(const Function& func, const std::string& body, std::string& out_body)
    {
        out_body = func(body);
    }

    template<typename Function, typename Self>
    static typename std::enable_if<std::is_void<typename std::result_of<Function(Self, const std::string&)>::type>::value>::type
    call_member_raw(const Function& func, Self* self, const std::string& body, std::string&)
    {
        (*self.*func)(body);
    }

    template<typename Function, typename Self>
    static typename std::enable_if<!std::is_void<typename std::result_of<Function(Self, const std::string&)>::type>::value>::type
    call_member_raw(const Function& func, Self* self, const std::string& body, std::string& out_body)
    {
        out_body = (*self.*func)(body);
    }

private:
    template<typename Function>
    class invoker
    {
    public:
        static void apply(const Function& func, const message_ptr& in_message, message_ptr& out_message)
        {
            try
            {
                call(func, in_message, out_message);
            }
            catch (std::exception& e)
            {
                log_warn(e.what());
            }
        }

        template<typename Self>
        static void apply_member(const Function& func, Self* self, const message_ptr& in_message, message_ptr& out_message)
        {
            try
            {
                call_member(func, self, in_message, out_message);
            }
            catch (std::exception& e)
            {
                log_warn(e.what());
            }
        }
    }; 

    template<typename Function>
    class invoker_raw
    {
    public:
        static void apply(const Function& func, const std::string& body, std::string& out_body)
        {
            try
            {
                call_raw(func, body, out_body);
            }
            catch (std::exception& e)
            {
                log_warn(e.what());
            }
        }

        template<typename Self>
        static void apply_member(const Function& func, Self* self, const std::string& body, std::string& out_body)
        {
            try
            {
                call_member_raw(func, self, body, out_body);
            }
            catch (std::exception& e)
            {
                log_warn(e.what());
            }
        }
    }; 

    struct pub_coming_helper
    {
        void operator()(const std::string& topic_name, const std::string& body, serialize_mode mode)
        {
            router::singleton::get()->publisher_coming_(topic_name, body, mode);
        }
    };

    struct sub_coming_helper
    {
        void operator()(const std::string& topic_name, const std::string& body, const connection_ptr& conn)
        {
            router::singleton::get()->subscriber_coming_(topic_name, body, conn);
        }
    };

private:
    template<typename Function>
    void bind_non_member_func(const std::string& protocol, const Function& func)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        invoker_map_[protocol] = { std::bind(&invoker<Function>::apply, func, 
                                                std::placeholders::_1, std::placeholders::_2) };
    }

    template<typename Function, typename Self>
    void bind_member_func(const std::string& protocol, const Function& func, Self* self)
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        invoker_map_[protocol] = { std::bind(&invoker<Function>::template apply_member<Self>, func, self, 
                                                std::placeholders::_1, std::placeholders::_2) };
    }

    template<typename Function>
    void bind_non_member_func_raw(const std::string& protocol, const Function& func)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        invoker_raw_map_[protocol] = { std::bind(&invoker_raw<Function>::apply, func, 
                                                std::placeholders::_1, std::placeholders::_2) };
    }

    template<typename Function, typename Self>
    void bind_member_func_raw(const std::string& protocol, const Function& func, Self* self)
    {
        std::lock_guard<std::mutex> lock(raw_map_mutex_);
        invoker_raw_map_[protocol] = { std::bind(&invoker_raw<Function>::template apply_member<Self>, func, self, 
                                                std::placeholders::_1, std::placeholders::_2) };
    }

public:
    using pub_comming_callback = std::function<void(const std::string&, const std::string&, serialize_mode)>;
    using sub_comming_callback = std::function<void(const std::string&, const std::string&, const connection_ptr&)>;
    pub_comming_callback publisher_coming_ = nullptr;
    sub_comming_callback subscriber_coming_ = nullptr;

private:
    thread_pool threadpool_;
    std::unordered_map<std::string, invoker_function> invoker_map_;
    std::unordered_map<std::string, invoker_function_raw> invoker_raw_map_;
    std::mutex map_mutex_;
    std::mutex raw_map_mutex_;
    pub_coming_helper pub_coming_helper_;
    sub_coming_helper sub_coming_helper_;
};

}
}

