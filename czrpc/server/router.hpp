#pragma once

#include "base/table/threadsafe_unordered_map.hpp"
#include "base/singleton.hpp"
#include "invoker.hpp"

using namespace czrpc::base::table;

namespace czrpc
{
namespace server
{
class router
{
    DEFINE_SINGLETON(router);
public:
    router() = default;

    template<typename Function>
    void bind(const std::string& protocol, const Function& func)
    {
        route_map_.replace(protocol, invoker_function{ [func](auto&& ...params) {
            invoker<Function>::apply(func, std::forward<decltype(params)>(params)...);
        }});
    }

    template<typename Function, typename Self>
    void bind(const std::string& protocol, const Function& func, Self* self)
    {
        route_map_.replace(protocol, invoker_function{ [func, self](auto&& ...params) {
            invoker<Function>::template apply_member<Self>(func, self, std::forward<decltype(params)>(params)...);
        }});
    }

    template<typename Function>
    void bind_raw(const std::string& protocol, const Function& func)
    {
        route_raw_map_.replace(protocol, invoker_function{ [func](auto&& ...params) {
            invoker<Function>::apply(func, std::forward<decltype(params)>(params)...);
        }});
    }

    template<typename Function, typename Self>
    void bind_raw(const std::string& protocol, const Function& func, Self* self)
    {
        route_raw_map_.replace(protocol, invoker_function{ [func, self](auto&& ...params) {
            invoker<Function>::template apply_member<Self>(func, self, std::forward<decltype(params)>(params)...);
        }});
    }

    void unbind(const std::string& protocol)
    {
        route_map_.erase(protocol);
    }

    bool is_bind(const std::string& protocol)
    {
        return route_map_.exists(protocol);
    }

    void unbind_raw(const std::string& protocol)
    {
        route_raw_map_.erase(protocol);
    }

    bool is_bind_raw(const std::string& protocol)
    {
        return route_raw_map_.exists(protocol);
    }

    bool route(const std::string& protocol, const request_ptr& req, const response_ptr& rsp)
    {
        invoker_function func;
        if (route_map_.find(protocol, func))
        {
            func(req, rsp);
            return true;
        }
        return false;
    }

    bool route_raw(const std::string& protocol, const request_ptr& req, const response_ptr& rsp)
    {
        invoker_function func;
        if (route_raw_map_.find(protocol, func))
        {
            func(req, rsp);
            return true;
        }
        return false;
    }

private:
    threadsafe_unordered_map<std::string, invoker_function> route_map_;
    threadsafe_unordered_map<std::string, invoker_function> route_raw_map_;
};

}
}

