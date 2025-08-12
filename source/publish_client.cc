#include "./client/rpc_client.hpp"

using namespace util_ns;
using namespace std;

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
    //3. 向主题发布消息
    for (int i = 0; i < 10; i++) {
        client->publish("hello", "Hello World-" + std::to_string(i));
    }
    client->shutdown();
    return 0;
}