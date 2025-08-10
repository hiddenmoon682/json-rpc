#include "./server/rpc_server.hpp"

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
    std::unique_ptr<server::SDescribeFactory> desc_factory(new server::SDescribeFactory());
    desc_factory->setMethodName("Add");
    desc_factory->setParamsDesc("num1", server::VType::INTEGRAL);
    desc_factory->setParamsDesc("num2", server::VType::INTEGRAL);
    desc_factory->setReturnType(server::VType::INTEGRAL);
    desc_factory->setCallback(Add);
    
    server::RpcServer server(Address("127.0.0.1", 8080));
    server.registerMethod(desc_factory->build());
    server.start();
    return 0;
}

// int main()
// {
//     auto conn = ConnectionFactory::create();

//     return 0;
// }