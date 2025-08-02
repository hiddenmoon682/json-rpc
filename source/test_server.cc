#include "./common/detail.hpp"
#include "./common/message.hpp"
#include "./common/net.hpp"
#include "./common/dispatcher.hpp"
#include "./server/rpc_router.hpp"

using namespace util_ns;
using namespace std;

void Add(const Json::Value& params, Json::Value& result)
{
    int num1 = params["num1"].asInt();
    int num2 = params["num2"].asInt();
    result = num1 + num2;
}

// 将rpcrouter和dispatcher联合在一起
int main()
{
    // 构造Dispatcher
    std::shared_ptr<Dispatcher> dsp = std::make_shared<Dispatcher>();
    // 构造RpcRouter
    std::shared_ptr<server::RpcRouter> router = std::make_shared<server::RpcRouter>();
    // 定义SDescribe工厂类，由它来生产SDescribe
    std::unique_ptr<server::SDescribeFactory> desc_factory(new server::SDescribeFactory());
    desc_factory->setMethodName("Add");
    desc_factory->setParamsDesc("num1", server::VType::INTEGRAL);
    desc_factory->setParamsDesc("num2", server::VType::INTEGRAL);
    desc_factory->setCallback(Add);
    // 向RpcRouter中注册RpcRequest请求回调函数
    router->registerMethod(desc_factory->build());

    // bind生成合适的onRpcRequest函数
    auto onRpcRequest = std::bind(&server::RpcRouter::onRpcRequest, router.get(), std::placeholders::_1, std::placeholders::_2);

    // 向DisPatcher中注册好针对RpcRequest请求的回调方法
    dsp->registerHandler<RpcRequest>(MType::REQ_RPC, onRpcRequest);
    // bind好要设置进服务器里的onMessageCallback
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