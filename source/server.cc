#include "./common/detail.hpp"
#include "./server/rpc_server.hpp"

using namespace util_ns;
using namespace std;

int main() 
{
    auto server = std::make_shared<server::TopicServer>(7070);
    server->start();
    return 0;
}