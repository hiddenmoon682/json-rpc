#include "./common/detail.hpp"
#include "./common/message.hpp"
#include "./common/net.hpp"
#include "./common/dispatcher.hpp"

using namespace util_ns;
using namespace std;

void onRpcRequest(const BaseConnection::ptr &conn, RpcRequest::ptr &msg)
{
    std::shared_ptr<util_ns::RpcResponse> rrs= util_ns::MessageFactory::create<util_ns::RpcResponse>();

    rrs->setRCode(RCode::RCODE_OK);
    Json::Value val;
    val["result"] = "11 + 22 = 33";
    rrs->setResult(val);
    rrs->SetId("2222");
    rrs->SetMytype(MType::RSP_RPC);
    conn->send(rrs);
    LOG(INFO, "成功发送\n");
}

int main()
{
    std::shared_ptr<Dispatcher> dsp = std::make_shared<Dispatcher>();
    dsp->registerHandler<RpcRequest>(MType::REQ_RPC, onRpcRequest);

    auto message_cb = std::bind(&Dispatcher::onMessage, dsp.get(), std::placeholders::_1, std::placeholders::_2);

    auto server = ServerFactory::create(8080);
    server->setMessageCallback(message_cb);
    server->start();

    return 0;
}

// int main()
// {
//     auto conn = ConnectionFactory::create();

//     return 0;
// }