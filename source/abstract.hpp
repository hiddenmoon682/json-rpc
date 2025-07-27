#pragma once

#include <string>
#include <memory>
#include <functional>
#include "fields.hpp"

namespace util_ns
{
    // 用于 描述数据包 的基类
    class BaseMessage
    {
    private:
        util_ns::MType _mtype; // 消息类型
        std::string _rid;      // 消息id
    public:
        // 给 std::shared_ptr<BaseMessage> 类型取别名为ptr
        using ptr = std::shared_ptr<BaseMessage>;

        virtual ~BaseMessage() {}

        // 设置ID
        virtual void SetId(const std::string &id)
        {
            _rid = id;
        }

        // 设置消息类型
        virtual void SetMytype(util_ns::MType mtype)
        {
            _mtype = mtype;
        }

        virtual std::string rid()
        {
            return _rid;
        }

        virtual util_ns::MType mtype()
        {
            return _mtype;
        }

        // 序列化   纯虚函数
        virtual std::string serialize() = 0;
        // 反序列化
        virtual bool unserialize(const std::string &msg) = 0;
        // 反序列化后对信息进行校验
        virtual bool check() = 0;
    };

    // 用于描述 缓冲区Buffer 的基类
    class BaseBuffer
    {
    private:
    public:
        // 给 std::shared_ptr<BaseBuffer> 类型取别名为ptr
        using ptr = std::shared_ptr<BaseBuffer>;
        // 返回当前缓冲区中的数据大小
        virtual size_t readableSize() = 0;
        // 获取但不删除前4个字节的内容
        virtual int32_t peekInt32() = 0;
        // 删除前4个字节的内容，通常在peek后使用
        virtual void retrieveInt32() = 0;
        // 获取并删除前4个字节的内容
        virtual int32_t readInt32() = 0;
        ;
        // 从缓冲区中取出指定长度的字符串
        virtual std::string retrieveAsString(size_t len) = 0;
    };

    // 用于 根据缓冲区中的内容和自定义协议，从缓冲区中取出完整的数据包 的基类
    class BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<BaseProtocol>;
        // 判断缓冲区中能否取出完整的数据包
        virtual bool canProcessed(const BaseBuffer::ptr &buf) = 0;
        // 从缓冲区中取出完整的数据包，参数1为缓冲区，参数2为输出的msg
        virtual bool onMessage(const BaseBuffer::ptr &buf, BaseMessage::ptr &msg) = 0;
        // 发送消息时对其进行序列化
        virtual std::string serialize(const BaseMessage::ptr &msg) = 0;
    };

    // 用于描述连接的基类
    class BaseConnection
    {
    public:
        using ptr = std::shared_ptr<BaseConnection>;
        // 发送消息
        virtual void send(const BaseMessage::ptr &msg) = 0;
        // 断开连接
        virtual void shutdown() = 0;
        // 判断连接状态
        virtual bool connected() = 0;
    };

    // 给 void -(const BaseConnection::ptr&) 这种函数类型取别名为ConnectionCallback
    using ConnectionCallback = std::function<void(const BaseConnection::ptr &)>;
    // ...
    using CloseCallback = std::function<void(const BaseConnection::ptr &)>;
    using MessageCallback = std::function<void(const BaseConnection::ptr &, BaseMessage::ptr &)>;

    // 用于 描述服务器Server 的基类
    class BaseServer
    {
    private:
        ConnectionCallback _cb_connection; // 连接建立的回调函数
        CloseCallback _cb_close;           // 连接断开的回调函数
        MessageCallback _cb_message;       // 收到消息的回调函数
    public:
        using ptr = std::shared_ptr<BaseServer>;
        // 设置对应的回调函数
        virtual void setConnectionCallback(const ConnectionCallback &cb)
        {
            _cb_connection = cb;
        }
        virtual void setCloseCallback(const CloseCallback &cb)
        {
            _cb_close = cb;
        }
        virtual void setMessageCallback(const MessageCallback &cb)
        {
            _cb_message = cb;
        }
        
        // 服务器启动
        virtual void start() = 0;
    };

    // 用于 描述客户端Client 的基类
    class BaseClient
    {
    private:
        ConnectionCallback _cb_connection; // 连接建立的回调函数
        CloseCallback _cb_close;           // 连接断开的回调函数
        MessageCallback _cb_message;       // 收到消息的回调函数
    public:
        using ptr = std::shared_ptr<BaseClient>;
        // 设置对应的回调函数
        virtual void setConnectionCallback(const ConnectionCallback &cb)
        {
            _cb_connection = cb;
        }
        virtual void setCloseCallback(const CloseCallback &cb)
        {
            _cb_close = cb;
        }
        virtual void setMessageCallback(const MessageCallback &cb)
        {
            _cb_message = cb;
        }
        
        // 连接服务器
        virtual void connect() = 0;
        // 关闭连接
        virtual void shutdown() = 0;
        // 发送消息
        virtual bool send(const BaseMessage::ptr&) = 0;
        // 获取当前连接
        virtual BaseConnection::ptr connection() = 0;
        // 判断连接状态
        virtual bool connected() = 0;
    };
};
