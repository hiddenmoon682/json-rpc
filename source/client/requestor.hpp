#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <future>
#include <functional>

namespace util_ns
{
    namespace client
    {
        // 因为服务端处理报文时都是进行异步处理，因此进行响应时是没有时序的，而对于客户端，如果一下子发送多个请求
        // 那么就可能无法确认哪个响应是针对哪个请求的，因此在报文中设置了ID字段，保证请求和响应中的ID字段相同，
        // 同时客户端就需要对自己发送出去的请求使用ID进行管理，保证请求响应一一对应
        // Requestor类就是客户端用于管理请求的
        class Requestor
        {
        public:
            using ptr = std::shared_ptr<Requestor>;
            using RequestCallback = std::function<void(const BaseMessage::ptr&)>;   // 回调处理响应的回调方法
            using AsyncResponse = std::future<BaseMessage::ptr>;    // ....
            // 对请求的描述
            struct RequestDescribe
            {
                using ptr = std::shared_ptr<RequestDescribe>;
                BaseMessage::ptr request;       // 请求本身
                RType rtype;                    // 处理响应方法：异步还是回调
                std::promise<BaseMessage::ptr> response;    // 用于异步结果传递
                RequestCallback callback;       // 回调函数
            };
            
        private:
            std::mutex _mutex;
            std::unordered_map<std::string, RequestDescribe::ptr> _request_desc;
        public:
            // 针对响应的处理
            // 第一个参数是连接，第二个参数是响应信息
            void onResponse(const BaseConnection::ptr& conn, BaseMessage::ptr& msg)
            {
                std::string rid = msg->rid();
                RequestDescribe::ptr rdp = getDescribe(msg->rid());
                if(rdp == RequestDescribe::ptr())
                {
                    LOG(FATAL, "收到响应 - %s，但是未找到对应的请求描述类!\n", rid.c_str());
                    return;
                }
                // 如果设置的是异步处理
                if(rdp->rtype == RType::REQ_ASYNC)
                {   
                    // ....还没有进行真正的处理
                    rdp->response.set_value(msg);   // 设置
                }
                // 如果设置的是回调处理
                else if(rdp->rtype == RType::REQ_CALLBACK)
                {
                    // 调用回调函数进行处理
                    if(rdp->callback)
                        rdp->callback(msg);
                }
                else
                {
                    LOG(WARING, "请求类型未知\n");
                }
                // 处理完后删掉请求描述类
                delDescribe(msg->rid());
            }

            // 异步调用(非阻塞)
            bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req, AsyncResponse &async_rsp)
            {
                // 构造请求描述类，插入hash表中进行管理
                RequestDescribe::ptr rdp = newDescribe(req, RType::REQ_ASYNC);
                if(rdp.get() == nullptr)
                {
                    LOG(FATAL, "构造请求描述对象失败！\n");
                    return false;
                }
                // 发送请求
                conn->send(req);

                // 将async_rsp这个future类关联到rdp->response这个promise类，等到其他线程的promise调用set_value后
                // async_rsp调用get()就可以获取结果了
                async_rsp = rdp->response.get_future();
                return true;
            }
            
            // 同步调用(阻塞)
            bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req, BaseMessage::ptr &rsp)
            {
                AsyncResponse rsp_future;
                // 异步发送数据
                bool ret = send(conn, req, rsp_future);
                if(ret == false)
                {
                    return false;
                }
                // 调用get()阻塞获取结果
                rsp = rsp_future.get();
                return true;
            }

            // 回调方法处理响应(非阻塞)，回调函数自动处理
            bool send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req, const RequestCallback &cb)
            {
                RequestDescribe::ptr rdp = newDescribe(req, RType::REQ_ASYNC, cb);
                if(rdp.get() == nullptr)
                {
                    LOG(FATAL, "构造请求描述对象失败！\n");
                    return false;
                }
                conn->send(req);
                return true;
            }

        private:
            // 增：创建一个新的描述请求类，设置好后插入hash表中
            RequestDescribe::ptr newDescribe(const BaseMessage::ptr& req,  
                                             RType rtype,  
                                             const RequestCallback& cb = RequestCallback())
            {
                RequestDescribe::ptr rd = std::make_shared<RequestDescribe>();
                rd->request = req;
                rd->rtype = rtype;
                // 如果是rtype规定是回调处理，那么顺便设置好rd->callback变量，否则不需要
                if(rtype == RType::REQ_CALLBACK && cb)
                {
                    rd->callback = cb;
                }
                std::unique_lock<std::mutex> lock(_mutex);
                _request_desc.insert(std::make_pair(req->rid(), rd));
                return rd;
            }

            // 查：获取一个请求描述类
            RequestDescribe::ptr getDescribe(const std::string& rid)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _request_desc.find(rid);
                if(it == _request_desc.end())
                {
                    return RequestDescribe::ptr();
                }
                return it->second;
            }

            // 删
            void delDescribe(const std::string& rid)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _request_desc.erase(rid);
            }
        };
    };
};