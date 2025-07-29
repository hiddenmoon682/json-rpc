#include "detail.hpp"
#include "message.hpp"
#include "net.hpp"

using namespace util_ns;
using namespace std;

void onMessage(const BaseConnection::ptr &conn, BaseMessage::ptr &msg)
{
    std::shared_ptr<util_ns::RpcResponse> rrs= util_ns::MessageFactory::create<util_ns::RpcResponse>();

    rrs->setRCode(RCode::RCODE_OK);
    Json::Value val;
    val["result"] = "11 + 22 = 33";
    rrs->setResult(val);
    conn->send(rrs);
    LOG(INFO, "成功发送\n");
}

int main()
{
    auto server = ServerFactory::create(8080);
    server->setMessageCallback(onMessage);
    server->start();

    return 0;
}

// int main()
// {
//     auto conn = ConnectionFactory::create();

//     return 0;
// }