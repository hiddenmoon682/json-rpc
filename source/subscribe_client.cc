#include "./server/rpc_server.hpp"
#include <thread>
using namespace util_ns;
using namespace std;

void callback(const std::string &key, const std::string &msg) {
    LOG(INFO, "%s 主题收到推送过来的消息： %s\n", key.c_str(), msg.c_str());
}

int main()
{
     //1. 实例化客户端对象
    auto client = std::make_shared<client::TopicClient>("127.0.0.1", 7070);
    LOG(DEBUG, "----------------\n");
    //2. 创建主题
    bool ret = client->create("hello");
    if (ret == false) {
        LOG(WARING, "创建主题失败！\n");

    }
    //3. 订阅主题
    ret = client->subscribe("hello", callback);
    //4. 等待->退出
    std::this_thread::sleep_for(std::chrono::seconds(10));
    client->shutdown();
    return 0;
}
