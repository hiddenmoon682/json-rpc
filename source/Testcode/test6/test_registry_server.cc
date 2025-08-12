#include "./common/detail.hpp"
#include "./server/rpc_server.hpp"

using namespace util_ns;
using namespace std;

int main() 
{
    server::RegistryServer reg_server(8899);
    reg_server.start();
    return 0;
}