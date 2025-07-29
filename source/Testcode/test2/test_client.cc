#include "detail.hpp"
#include "message.hpp"
#include "net.hpp"
#include <thread>

using namespace util_ns;
using namespace std;

void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg)
{
    cout << msg->serialize() << endl;
    LOG(INFO, "成功接收\n");
}

int main()
{
    std::shared_ptr<util_ns::RpcRequest> rrp = util_ns::MessageFactory::create<util_ns::RpcRequest>();

    rrp->setMethod("news");
    Json::Value val;
    val["num1"] = 11;
    val["num2"] = 22;
    rrp->setParms(val);
    rrp->SetId("1111");
    rrp->SetMytype(MType::REQ_RPC);

    cout << rrp->check() << endl;

    auto client = ClientFactory::create("127.0.0.1",8080);
    client->setMessageCallback(onMessage);
    client->connect();
    client->send(rrp);

    std::this_thread::sleep_for(std::chrono::seconds(10));

    client->shutdown();

    LOG(INFO, "安全结束\n");

    return 0;
}