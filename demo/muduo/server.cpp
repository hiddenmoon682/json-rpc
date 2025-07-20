#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <iostream>
#include <string>
#include <unordered_map>

class DictServer
{
private:
    muduo::net::EventLoop _baseloop;    // 事件监听
    muduo::net::TcpServer _server;      // 服务器
private:
    // 连接建立回调
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if(conn->connected())
            std::cout << "连接建立！\n";
        else
            std::cout << "连接断开！\n";
    }
    // 收到消息回调
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf)
    {
        static std::unordered_map<std::string, std::string> dict_map = {
                {"hello",  "你好"},
                {"world",  "世界"},
                {"bite",  "比特"}
            };
        std::string msg = buf->retrieveAllAsString();
        std::string res;
        auto it = dict_map.find(msg);
        if(it != dict_map.end())
        {
            res = it->second;
        }
        else
        {
            res = "未知单词";
        }
        conn->send(res);
    }
public:
    DictServer(int port)
        :_server(&_baseloop, 
            muduo::net::InetAddress("0.0.0.0", port), 
            "DictServer", 
            muduo::net::TcpServer::kReusePort)
        {
            // 建立连接事件回调
            _server.setConnectionCallback(std::bind(&DictServer::onConnection, this, std::placeholders::_1));
            // 建立连接消息回调
            _server.setMessageCallback(std::bind(&DictServer::onMessage, this, 
                std::placeholders::_1, std::placeholders::_2));
        }

    void start()
    {
        _server.start();//先开始监听
        _baseloop.loop();//开始死循环事件监控
    }
};

int main()
{
    DictServer server(9090);
    server.start();
    return 0;
}

