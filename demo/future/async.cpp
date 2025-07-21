#include <iostream>
#include <future>

using namespace std;

int Add(int num1, int num2) {
    std::cout << "into add!\n";
    return num1 + num2;
}

int main()
{
    //std::launch::async策略：内部创建一个线程执行函数，函数运行结果通过future获取
    //std::launch::deferred策略：同步策略，获取结果的时候再去执行任务
    std::future<int> res = std::async(std::launch::async, Add, 11, 22);
    //std::future<int>::get()  用于获取异步任务的结果，如果还没有结果就会阻塞
    cout << res.get() << endl;

    return 0;
}

