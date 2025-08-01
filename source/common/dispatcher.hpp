#pragma once

#include "detail.hpp"
#include "message.hpp"
#include "net.hpp"

// using MessageCallback = std::function<void(const BaseConnection::ptr &, BaseMessage::ptr &)>;
// 当上层调用registerHandler函数来注册对应的方法时，它写的onMessage函数是 MessageCallback 类型的
// 而MessageCallback类型传入的第二个参数是BassMessage::ptr类型的，这是基类(父类)类型，它无法调用子类的方法，比如说method()等方法，这就会对上层造成不便
// 同时对于Dispatcher中的_handlers容器，它是unordered_map类型的，它只能存储一种类型，不能同时存储RpcRequest类型和RpcResponse类型，
// 因此DisPatcher类不能使用模板，而是使用CallBack来封装MessageCallback，
// 使用模板做到能够让MessageCallBack的参数能够根据不同参数转变

// 使用Callback作为基类去继承出CallbackT类，因为std::unordered_map<MType, Callback<T>>也是不行的，要再来一层屏蔽掉T

namespace util_ns
{
    // 基类，用Callback去封装MessageCallBack
    class Callback
    {
    public:
        using ptr = std::shared_ptr<Callback>;
        virtual void onMessage(const BaseConnection::ptr& conn, BaseMessage::ptr& msg) = 0;
    };
    
    template<typename T>
    class CallbackT : public Callback
    {
    public:
        // 因为传入的T类型一定是类类型用于后面的基类转子类类型，而不是指针类型，所以这里要std::shared_ptr<T>
        using MessageCallbackT = std::function<void(const BaseConnection::ptr& conn, std::shared_ptr<T>& msg)>;
    private:
        MessageCallbackT _handler;   // 上层实际注册进来的函数
    public:
        using ptr = std::shared_ptr<CallbackT>;

        CallbackT(const MessageCallbackT& handler)
            : _handler(handler)
        {}

        void onMessage(const BaseConnection::ptr& conn, BaseMessage::ptr& msg) override
        {
            auto type_msg = std::dynamic_pointer_cast<T>(msg);
            _handler(conn, type_msg);
        }
    };

    // Dispatcher分发模块
    class Dispatcher
    {
    private:
        std::mutex _mutex;  // 保证线程安全
        std::unordered_map<MType, Callback::ptr> _handlers;   // 类型和回调函数的映射关系
    public:
        using ptr = std::shared_ptr<Dispatcher>;
        
        // 注册消息类型所对应的回调函数,需要上层设置对应的回调函数的类型
        // typename是面对模板类时表面这是个类型而不是变量或其他
        template<typename T>
        void registerHandler(MType mtype, const typename CallbackT<T>::MessageCallbackT& handler)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            std::shared_ptr<CallbackT<T>> cb = std::make_shared<CallbackT<T>>(handler);
            _handlers.insert(std::make_pair(mtype, cb));
        }

        // 根据不同的消息类型，调用对应的回调方法
        // 该函数要设置进BaseConnection的onMessageCallback中
        void onMessage(const BaseConnection::ptr& conn, BaseMessage::ptr& msg)
        {
            // 找到消息类型对应的业务处理函数，调用即可
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _handlers.find(msg->mtype());
            if(it != _handlers.end())
            {
                // 找到了,直接调用对应的函数
                it->second->onMessage(conn, msg);
                return;
            }
            LOG(FATAL, "收到未知类型消息");
            conn->shutdown();
            return;
        }
    };
    
};