#pragma once

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/base/CountDownLatch.h>

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "abstract.hpp"
#include "fields.hpp"
#include "detail.hpp"
#include "message.hpp"

namespace util_ns
{
    class MuduoBuffer : public BaseBuffer
    {
    private:
        muduo::net::Buffer *_buf;

    public:
        using ptr = std::shared_ptr<BaseBuffer>;
        // 构造函数
        MuduoBuffer(muduo::net::Buffer *buf) : _buf(buf) {}

        // 返回当前缓冲区中的数据大小
        virtual size_t readableSize() override
        {
            return _buf->readableBytes();
        }
        // 获取但不删除前4个字节的内容
        virtual int32_t peekInt32() override
        {
            // muduo库是一个网络库，从缓冲区取出一个4字节整形，会进行网络字节序的转换
            // 所以在向buffer里填充数据时需要奖主机字节序转换成网络字节序
            return _buf->peekInt32();
        }
        // 删除前4个字节的内容，通常在peek后使用
        virtual void retrieveInt32() override
        {
            return _buf->retrieveInt32();
        }
        // 获取并删除前4个字节的内容
        virtual int32_t readInt32() override
        {
            return _buf->readInt32();
        }
        // 从缓冲区中取出指定长度的字符串
        virtual std::string retrieveAsString(size_t len) override
        {
            return _buf->retrieveAsString(len);
        }
    };

    class BufferFactory
    {
    public:
        template <typename... Args>
        static BaseBuffer::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
        }
    };

    class LVProtocol : public BaseProtocol
    {
    private:
        const size_t lenFieldsLength = 4;
        const size_t mtypeFieldsLength = 4;
        const size_t idlenFieldsLength = 4;

    public:
        // |--Len--|--mtype--|--idlen--|--id--|--body--|
        // Len表示消息总长度，不包括字节的4字节
        using ptr = std::shared_ptr<BaseProtocol>;
        // 判断缓冲区中能否取出一条完整的数据包
        virtual bool canProcessed(const BaseBuffer::ptr &buf) override
        {
            if (buf->readableSize() < lenFieldsLength) // 小于4字节，直接false
            {
                return false;
            }
            int32_t total_len = buf->peekInt32();
            if (buf->readableSize() < (total_len + lenFieldsLength))
            {
                return false;
            }
            return true;
        }
        // 从缓冲区中取出完整的数据包，参数1为缓冲区，参数2为输出的msg
        virtual bool onMessage(const BaseBuffer::ptr &buf, BaseMessage::ptr &msg) override
        {
            // 当调用onMessage的时候，默认认为缓冲区中的数据足够一条完整的消息
            int32_t total_len = buf->readInt32();                                         // 获取总长度
            MType mtype = (MType)buf->readInt32();                                        // 获取消息类型
            int32_t idlen = buf->readInt32();                                             // 获取id长度
            int32_t body_len = total_len - idlen - idlenFieldsLength - mtypeFieldsLength; // 获取正文长度
            std::string id = buf->retrieveAsString(idlen);
            std::string body = buf->retrieveAsString(body_len);
            msg = MessageFactory::create(mtype);
            if (msg.get() == nullptr)
            {
                LOG(FATAL, "消息类型错误，构造消息对象失败！\n");
                return false;
            }
            bool ret = msg->unserialize(body);
            if (ret == false)
            {
                LOG(FATAL, "消息正文反序列化失败！\n");
                return false;
            }
            msg->SetId(id);
            msg->SetMytype(mtype);
            return true;
        }
        // 发送消息时对其进行序列化, 注意对前三个字段从主机字节序转换成网络字节序
        virtual std::string serialize(const BaseMessage::ptr &msg)
        {
            // |--Len--|--mtype--|--idlen--|--id--|--body--|
            std::string body = msg->serialize();
            std::string id = msg->rid();
            auto mtype = htonl((int32_t)msg->mtype());
            int32_t idlen = htonl(id.size());
            int32_t h_total_len = mtypeFieldsLength + idlenFieldsLength + id.size() + body.size(); // 主机字节序
            int32_t n_total_len = htonl(h_total_len);                                              // 网络字节序

            std::string result;
            result.reserve(h_total_len + lenFieldsLength);
            result.append((char *)&n_total_len, lenFieldsLength);
            result.append((char *)&mtype, mtypeFieldsLength);
            result.append((char *)&idlen, idlenFieldsLength);
            result.append(id);
            result.append(body);

            return result;
        }
    };

    class ProtocolFactory
    {
    public:
        template <typename... Args>
        static BaseProtocol::ptr create(Args &&...args)
        {
            return std::make_shared<LVProtocol>(std::forward<Args>(args)...);
        }
    };

    class MuduoConnection : public BaseConnection
    {
    private:
        BaseProtocol::ptr _protocol;
        muduo::net::TcpConnectionPtr _conn;

    public:
        MuduoConnection(const muduo::net::TcpConnectionPtr &conn, const BaseProtocol::ptr &protocol)
            : _conn(conn), _protocol(protocol)
        {}

        using ptr = std::shared_ptr<MuduoConnection>;
        // 发送消息
        virtual void send(const BaseMessage::ptr &msg) override
        {
            std::string body = _protocol->serialize(msg);
            _conn->send(body);
        };
        // 断开连接
        virtual void shutdown() override
        {
            _conn->shutdown();
        }
        // 判断连接状态
        virtual bool connected() override
        {
            return _conn->connected();
        }
    };

    class ConnectionFactory
    {
    public:
        template <typename... Args>
        static BaseConnection::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoConnection>(std::forward<Args>(args)...);
        }
    };

    class MuduoServer : public BaseServer
    {
    private:
        const size_t maxDataSize = (1 << 16); // 最大数据量
        muduo::net::EventLoop _baseloop;      // 事件监听
        muduo::net::TcpServer _server;        // 服务器
        std::mutex _mutex;
        BaseProtocol::ptr _protocol;                                                  // 自定义协议处理工具
        std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::ptr> _conns; // 管理连接
    public:
        using ptr = std::shared_ptr<MuduoServer>;

        MuduoServer(int port)
            : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "MuduoServer", muduo::net::TcpServer::kReusePort), 
            _protocol(ProtocolFactory::create())
        {}

        // 服务器启动
        virtual void start() override
        {
            // 建立连接事件回调
            _server.setConnectionCallback(std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));
            // 建立收到消息回调
            _server.setMessageCallback(std::bind(&MuduoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            // 先开始服务器
            _server.start();
            // 再开始死循环事件监听
            _baseloop.loop();
        }

    private:
        // 当有新连接到来或连接断开
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            // 新连接到来
            if (conn->connected())
            {
                LOG(INFO, "连接建立\n");
                // 创建新的MuduoConnection并添加进_conns进行管理
                // 可能有多个连接同时到来，上锁保证线程安全
                auto muduo_conn = ConnectionFactory::create(conn, _protocol);
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _conns.insert(std::make_pair(conn, muduo_conn));
                }
                // 调用连接处理函数
                if (_cb_connection)
                    _cb_connection(muduo_conn);
            }
            // 连接断开
            else
            {
                LOG(INFO, "连接断开\n");
                // 删除对连接的管理
                BaseConnection::ptr muduo_conn;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if (it == _conns.end())
                    {
                        return;
                    }
                    muduo_conn = it->second;
                    // 调用连接处理函数
                    if (_cb_close)
                        _cb_close(muduo_conn);
                }
            }
        }

        // 当有新的消息到来
        void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            LOG(INFO, "连接有数据到来，开始处理\n");
            auto base_buf = BufferFactory::create(buf);
            while (1)
            {
                if (_protocol->canProcessed(base_buf) == false)
                {
                    // 缓冲区数据不足以提取一条完整的报文
                    LOG(WARING, "缓冲区数据不足以提取一条完整的报文\n");
                    if (base_buf->readableSize() > maxDataSize)
                    {
                        // 缓冲区数据过多, 证明缓冲区中的数据出现问题，断开连接
                        conn->shutdown();
                        LOG(WARING, "缓冲区中数据过大\n");
                        return;
                    }
                    break;
                }
                // 能够处理数据
                BaseMessage::ptr msg;
                bool ret = _protocol->onMessage(base_buf, msg);
                if (ret == false)
                {
                    conn->shutdown();
                    LOG(WARING, "缓冲区中数据有误\n");
                    return;
                }
                // 反序列化成功，获得数据
                BaseConnection::ptr base_conn;

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if (it == _conns.end())
                    {
                        conn->shutdown();
                        return;
                    }
                    base_conn = it->second;
                }
                // 调用消息处理回调函数
                if (_cb_message)
                    _cb_message(base_conn, msg);
            }
        }
    };

    class ServerFactory
    {
    public:
        template <typename... Args>
        static BaseServer::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }
    };

    class MuduoClient : public BaseClient
    {
    private:
        const size_t maxDataSize = (1 << 16);    // 最大数据量
        BaseConnection::ptr _conn;               // 连接对象
        muduo::CountDownLatch _downlatch;        // 同步计数
        muduo::net::EventLoopThread _loopthread; // 监听线程（epoll）
        muduo::net::EventLoop *_baseloop;        // 对应的指针
        muduo::net::TcpClient _client;           // 客户端
        BaseProtocol::ptr _protocol;             // 处理自定义协议工具
    public:
        using ptr = std::shared_ptr<MuduoClient>;
        
        MuduoClient(const std::string &sip, int sport)
        : _baseloop(_loopthread.startLoop()),
          _downlatch(1), // 初始化为1，因为为0时wait()才会唤醒
          _client(_baseloop, muduo::net::InetAddress(sip, sport), "MuduoClient"),
          _protocol(ProtocolFactory::create())
        {}

        // 连接服务器
        virtual void connect() override
        {
            LOG(INFO, "设置回调函数，连接服务器\n");
            // 设置连接事件（连接建立/管理）的回调
            _client.setConnectionCallback(std::bind(&MuduoClient::onConnection, this, std::placeholders::_1));
            // 设置连接消息的回调
            _client.setMessageCallback(std::bind(&MuduoClient::onMessage, this,std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

            // 连接服务器
            _client.connect();
            _downlatch.wait();
            LOG(INFO, "服务器连接成功\n");
        }
        // 关闭连接
        virtual void shutdown() override
        {
            _client.disconnect();
        }
        // 发送消息
        virtual bool send(const BaseMessage::ptr& msg) override
        {
            if(connected() == false)
            {
                LOG(WARING, "连接已断开\n");
                return false;
            }
            _conn->send(msg);
            LOG(INFO, "成功发送数据\n");
            return true;
        }
        // 获取当前连接
        virtual BaseConnection::ptr connection() override
        {
            return _conn;
        }
        // 判断连接状态
        virtual bool connected() override
        {
            return (_conn && _conn->connected());
        }
    private:
        // 当有新连接到来或连接断开
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                LOG(INFO, "连接建立！\n");

                // 真正用于发送数据的对象是TcpConnection，所以要设置TcpConnectionPtr
                _conn = ConnectionFactory::create(conn, _protocol);
                // 因为进行connect操作时是非阻塞的，为了保证连接完成后才能发送消息，使用countdownlatch进行同步
                _downlatch.countDown(); // 计数--，为0时唤醒阻塞
            }
            else
            {
                LOG(INFO, "连接断开！\n");
                _conn.reset();  // 连接清理
            }
        }

        // 当有新的消息到来
        void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            LOG(INFO, "连接有数据到来，开始处理\n");
            auto base_buf = BufferFactory::create(buf);
            while (1)
            {
                if (_protocol->canProcessed(base_buf) == false)
                {
                    // 缓冲区数据不足以提取一条完整的报文
                    if (base_buf->readableSize() > maxDataSize)
                    {
                        // 缓冲区数据过多, 证明缓冲区中的数据出现问题，断开连接
                        conn->shutdown();
                        LOG(WARING, "缓冲区中数据过大\n");
                        return;
                    }
                    break;
                }
                // 能够处理数据
                BaseMessage::ptr msg;
                bool ret = _protocol->onMessage(base_buf, msg);
                if (ret == false)
                {
                    conn->shutdown();
                    LOG(WARING, "缓冲区中数据有误\n");
                    return;
                }
                // 反序列化成功，获得数据
                // 调用消息处理回调函数
                if (_cb_message)
                {
                    _cb_message(_conn, msg);
                }
                    
            }
        }
    };

    class ClientFactory {
        public:
            template<typename ...Args>
            static BaseClient::ptr create(Args&& ...args) {
                return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
            }
    };
}
