#include "./client/rpc_client.hpp"
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
    client::RpcClient client(true, "127.0.0.1", 8899);
    Json::Value params, result;
    params["num1"] = 11;
    params["num2"] = 22;
    bool ret = client.call("Add", params, result);
    if (ret != false) {
        LOG(INFO, "result: %d\n", result.asInt());
    }

    client::RpcCaller::JsonAsyncResponse res_future;
    params["num1"] = 33;
    params["num2"] = 44;
    ret = client.call("Add", params, res_future);
    if (ret != false) {
        result = res_future.get();
        LOG(INFO, "result: %d\n", result.asInt());
    }

    params["num1"] = 55;
    params["num2"] = 66;
    ret = client.call("Add", params, JsonCallback);
    LOG(INFO, "-------\n");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;

}