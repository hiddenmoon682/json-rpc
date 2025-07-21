#include <iostream>
#include <future>

using namespace std;

int Add(int num1, int num2) {
    std::cout << "into add!\n";
    return num1 + num2;
}

int main()
{
    // // 方案1：
    // //1. 封装任务
    // std::packaged_task<int(int, int)> task(Add);
    // //2. 获取 future
    // auto res = task.get_future(); 
    // // 在新线程中执行任务
    // // std::thread t(move(task), n1, n2 ...);
    // std::thread t(std::move(task), 11, 22);

    //  //3. 获取结果
    // std::cout << res.get() << std::endl;
    // t.join();

    // 方案二
    //1. 封装任务
    auto task = std::make_shared<std::packaged_task<int(int, int)>>(Add);

    //2. 获取任务包关联的future对象
    std::future<int> res = task->get_future();

    std::thread thr([task](){
        (*task)(11, 22);
    });


    //3. 获取结果
    std::cout << res.get() << std::endl;
    thr.join();

    return 0;
}

