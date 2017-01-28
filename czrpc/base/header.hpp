#pragma once
#include <string>
#include <unordered_map>

namespace czrpc
{
namespace base
{
constexpr const int max_buffer_len = 20 * 1024 * 1024; // 20MB
constexpr const int request_header_len = 4 + 4 + 4 + 4 + 4 + 4;
constexpr const int response_header_len = 4 + 4 + 4 + 4;
constexpr const int push_header_len = 4 + 4 + 4 + 4;
const std::string subscribe_topic_flag = "1";
const std::string cancel_subscribe_topic_flag = "0";
const std::string heartbeats_flag = "00";
const int heartbeats_milli = 3000;
const int connection_timeout_milli = 30000;
const int connection_timeout_sec = 30;
const int check_request_timeout_milli = 1000;

enum class serialize_mode : unsigned int
{
    serialize,
    non_serialize
};

enum class client_type : unsigned int
{
    rpc_client,
    async_rpc_client,
    pub_client,
    sub_client
};

struct client_flag
{
    serialize_mode mode;
    client_type type;
};

struct request_header
{
    unsigned int call_id_len;
    unsigned int protocol_len;
    unsigned int message_name_len;
    unsigned int body_len;
    client_flag flag;
};

struct request_content
{
    std::string call_id;
    std::string protocol;
    std::string message_name;
    std::string body;
};

struct request_data
{
    request_header header;
    request_content content;
};

enum class rpc_error_code : int 
{
    ok = 0,
    route_failed = 1
};

struct response_header
{
    unsigned int call_id_len;
    unsigned int message_name_len;
    unsigned int body_len;
    rpc_error_code error_code;
};

struct response_content
{
    std::string call_id;
    std::string message_name;
    std::string body;
};

struct response_data
{
    response_header header;
    response_content content;
};

struct push_header
{
    unsigned int protocol_len;
    unsigned int message_name_len;
    unsigned int body_len;
    serialize_mode mode;
};

struct push_content
{
    std::string protocol;
    std::string message_name;
    std::string body;
};

struct push_data
{
    push_header header;
    push_content content;
};

struct endpoint
{
    std::string ip;
    unsigned short port;
};

static std::unordered_map<int, std::string> rpc_error_map
{
    {static_cast<int>(rpc_error_code::ok), "OK"},
    {static_cast<int>(rpc_error_code::route_failed), "Route failed"}
};

static std::string get_rpc_error_string(rpc_error_code error_code)
{
    auto iter = rpc_error_map.find(static_cast<int>(error_code));
    if (iter != rpc_error_map.end())
    {
        return iter->second;
    }
    return "";
}

}
}

