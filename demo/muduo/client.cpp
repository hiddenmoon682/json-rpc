#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/CountDownLatch.h>

#include <iostream>
#include <string>
#include <memory>

class DictClient
{
private:
    muduo::net::TcpConnectionPtr _conn;      // 连接对象
    muduo::CountDownLatch _downlatch;        // 同步计数
    muduo::net::EventLoopThread _loopthread; // 监听线程（epoll）
    muduo::net::EventLoop *_baseloop;        // 对应的指针
    muduo::net::TcpClient _client;           // 客户端

private:
    // 建立连接回调
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            std::cout << "连接建立！\n";

            // 因为进行connect操作时是非阻塞的，为了保证连接完成后才能发送消息，使用countdownlatch进行同步
            _downlatch.countDown(); // 计数--，为0时唤醒阻塞
            // 真正用于发送数据的对象是TcpConnection，所以要设置TcpConnectionPtr
            _conn = conn;
        }
        else
        {
            std::cout << "连接断开！\n";
            _conn.reset();  // 连接清理
        }
    }

    // 收到消息回调
    // Timestamp可以省略
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
    {
        std::string res = buf->retrieveAllAsString();
        std::cout << res << std::endl;
    }

public:
    // 因为EventLoop在start之后是死循环的，所以再开一个专门用于监听的线程EventLoopThread，start()会返回EventLoop*指针
    // 传入IP地址和端口号
    DictClient(const std::string &sip, int sport)
        : _baseloop(_loopthread.startLoop()),
          _downlatch(1), // 初始化为1，因为为0时wait()才会唤醒
          _client(_baseloop, muduo::net::InetAddress(sip, sport), "DictClient")
    {
        // 设置连接事件（连接建立/管理）的回调
        _client.setConnectionCallback(std::bind(&DictClient::onConnection, this, std::placeholders::_1));
        // 设置连接消息的回调
        _client.setMessageCallback(std::bind(&DictClient::onMessage, this,
                                             std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 连接服务器
        _client.connect();
        _downlatch.wait();
    }

    bool send(const std::string &msg)
    {
        if (_conn->connected() == false)
        {
            std::cout << "连接已经断开，发送数据失败！\n";
            return false;
        }
        _conn->send(msg);
        return true;
    }
};

int main()
{
    DictClient client("127.0.0.1", 9090);
    while(1) {
        std::string msg;
        std::cin >> msg;
        client.send(msg);
    }
    return 0;
}