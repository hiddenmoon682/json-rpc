#include "./common/detail.hpp"
#include "./common/message.hpp"
#include "./common/net.hpp"
#include "./common/dispatcher.hpp"
#include "./client/requestor.hpp"
#include "./client/rpc_caller.hpp"
#include <thread>

using namespace util_ns;
using namespace std;

void JsonCallback(const Json::Value& val)
{
    LOG(DEBUG, "result = %d\n", val.asInt());
}

// 测试rpcCaller的三种rpc请求call方法
int main()
{
    // 不需要构建请求，只需要提供参数，caller会自己根据参数构建请求
    // 构建Requestor
    std::shared_ptr<client::Requestor> requestor = std::make_shared<client::Requestor>();
    // 构建Caller
    std::shared_ptr<client::RpcCaller> Caller = std::make_shared<client::RpcCaller>(requestor);

    // 构建DisPatcher
    std::shared_ptr<Dispatcher> dsp = std::make_shared<Dispatcher>();

    // 将Requestor中的onResponse设置进Dispatcher
    auto onRpcResponse = std::bind(&client::Requestor::onResponse, requestor.get(), std::placeholders::_1, std::placeholders::_2);
    dsp->registerHandler<BaseMessage>(MType::RSP_RPC, onRpcResponse);

    // 将Dispatcher中的onResponse设置进client
    auto message_cb = std::bind(&Dispatcher::onMessage, dsp.get(), std::placeholders::_1, std::placeholders::_2);
    auto client = ClientFactory::create("127.0.0.1",8080);
    client->setMessageCallback(message_cb);
    client->connect();

    // 获取client的连接
    auto conn = client->connection();
    // 设置方法名称和参数
    std::string name = "Add";
    Json::Value val;
    val["num1"] = 11;
    val["num2"] = 22;
    // 定义用于接收结果的变量
    Json::Value result;
    // 发起同步请求
    Caller->call(conn, name, val, result);

    LOG(DEBUG, "result = %d\n", result.asInt());

    // 发起异步请求
    // auto fresult = std::make_shared<std::future<Json::Value>>();
    // std::future<Json::Value> fresult;
    client::RpcCaller::JsonAsyncResponse fresult;
    val["num1"] = 33;
    val["num2"] = 44;
    Caller->call(conn, name, val, fresult);
    LOG(DEBUG, "-----------4-----------\n");
    result = fresult.get();
    LOG(DEBUG, "fresult = %d\n", result.asInt());
    LOG(DEBUG, "-----------5-----------\n");
    // 发起回调方法请求
    val["num1"] = 11;
    val["num2"] = 77;
    Caller->call(conn, name, val, JsonCallback);
    LOG(DEBUG, "-----------6-----------\n");

    std::this_thread::sleep_for(std::chrono::seconds(10));

    LOG(DEBUG, "安全结束\n");

    client->shutdown();

    return 0;
}