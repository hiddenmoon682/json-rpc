#pragma once
#include <string>
#include <unordered_map>

/*
    应用层自定义协议格式
    Len         4B  数据包长度
    mtype       4B  消息类型，如rpc请求/响应类型、主题创建/删除等类型
    idlength    4B  描述ID字段的实际长度
    MID             用于唯一标识消息，长度不固定
    body            数据包的正文字段

*/

namespace util_ns
{
// 应用层里自定义协议中的各种字段的宏

// 请求中的字段
#define KEY_METHOD "method"         // 方法名称
#define KEY_PARAMETERS "parameters" // 方法参数？
#define KEY_TOPIC_KEY "topic_key"   // 主题名称
#define KEY_TOPIC_MSG "topic_msg"   // 主题信息
#define KEY_OPTYPE "optype"         // 主题操作类型
#define KEY_HOST "host"             // 主机名称
#define KEY_IP "ip"                 // 主机ip地址
#define KEY_PORT "port"             // 主机端口

// 响应字段
#define KEY_RCODE "rcode"   // 响应码
#define KEY_RESULT "result" // 响应结果

    // 消息类型定义
    /*
    • Rpc请求&响应
    • 主题操作请求&响应：
    • 消息发布请求&响应
    • 服务操作请求&响应：
    */
    enum class MType
    {
        REQ_RPC = 0,
        RSP_RPC,
        REQ_TOPIC,
        RSP_TOPIC,
        REQ_SERVICE,
        RSP_SERVICE
    };

    // 响应码类型定义
    /*
    • 成功处理
    • 解析失败
    • 消息中字段缺失或错误导致无效消息
    • 连接断开
    • 无效的Rpc调用参数
    • Rpc服务不存在
    • 无效的Topic操作类型
    • 主题不存在
    • 无效的服务操作类型
    */

    enum class RCode
    {
        RCODE_OK = 0,
        RCODE_PARSE_FAILED,
        RCODE_ERROR_MSGTYPE,
        RCODE_INVALID_MSG,
        RCODE_DISCONNECTED,
        RCODE_INVALID_PARAMS,
        RCODE_NOT_FOUND_SERVICE,
        RCODE_INVALID_OPTYPE,
        RCODE_NOT_FOUND_TOPIC,
        RCODE_INTERNAL_ERROR
    };
    static std::string errReason(RCode code)
    {
        static std::unordered_map<RCode, std::string> err_map = {
            {RCode::RCODE_OK, "成功处理！"},
            {RCode::RCODE_PARSE_FAILED, "消息解析失败！"},
            {RCode::RCODE_ERROR_MSGTYPE, "消息类型错误！"},
            {RCode::RCODE_INVALID_MSG, "无效消息"},
            {RCode::RCODE_DISCONNECTED, "连接已断开！"},
            {RCode::RCODE_INVALID_PARAMS, "无效的Rpc参数！"},
            {RCode::RCODE_NOT_FOUND_SERVICE, "没有找到对应的服务！"},
            {RCode::RCODE_INVALID_OPTYPE, "无效的操作类型"},
            {RCode::RCODE_NOT_FOUND_TOPIC, "没有找到对应的主题！"},
            {RCode::RCODE_INTERNAL_ERROR, "内部错误！"}};
        auto it = err_map.find(code);
        if (it == err_map.end())
        {
            return "未知错误！";
        }
        return it->second;
    }

    // RPC请求类型
    /*
        同步请求
        异步请求
        回调请求
    */
    enum class RType
    {
        REQ_ASYNC = 0,
        REQ_CALLBACK
    };

    // 主题操作类型
    /*
        主题创建
        主题删除
        主题订阅
        主题取消订阅
        主题消息发布
    */
    enum class TopicOptype
    {
        TOPIC_CREATE = 0,
        TOPIC_REMOVE,
        TOPIC_SUBSCRIBE,
        TOPIC_CANCEL,
        TOPIC_PUBLISH
    };

    // 服务操作类型
    /*
        服务注册
        服务发现
        服务上线
        服务下线
    */
    enum class ServiceOptype
    {
        SERVICE_REGISTRY = 0,
        SERVICE_DISCOVERY,
        SERVICE_ONLINE,
        SERVICE_OFFLINE,
        SERVICE_UNKNOW
    };


    


}