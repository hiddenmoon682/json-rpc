#include "./common/detail.hpp"
#include "./common/message.hpp"
#include "./common/net.hpp"
#include "./common/dispatcher.hpp"
#include <thread>

using namespace util_ns;
using namespace std;

void onRpcRespnse(const BaseConnection::ptr &conn, RpcResponse::ptr &msg)
{
    cout << msg->serialize() << endl;
    LOG(INFO, "成功接收\n");
}

//////////////////////////////// 
//      待测试Dispatcher     ///
///////////////////////////////

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

    std::shared_ptr<Dispatcher> dsp = std::make_shared<Dispatcher>();
    dsp->registerHandler<RpcResponse>(MType::RSP_RPC, onRpcRespnse);

    auto message_cb = std::bind(&Dispatcher::onMessage, dsp.get(), std::placeholders::_1, std::placeholders::_2);

    auto client = ClientFactory::create("127.0.0.1",8080);

    client->setMessageCallback(message_cb);
    client->connect();
    client->send(rrp);

    std::this_thread::sleep_for(std::chrono::seconds(10));

    client->shutdown();

    LOG(INFO, "安全结束\n");

    return 0;
}