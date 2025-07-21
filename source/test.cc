#include "detail.hpp"

using namespace util_ns;
using namespace std;

int main()
{
    for(int i = 0; i < 10; i++)
    {
        cout << UUID::uuid() << endl;
    }

    return 0;
}