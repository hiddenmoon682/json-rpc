#pragma once
#include "requestor.hpp"

namespace util_ns
{
    namespace client
    {
        // 给上层提供进行rpc请求的接口
        class RpcCaller
        {
        public:
            using ptr = std::shared_ptr<RpcCaller>;
            using JsonAsyncResponse = std::future<Json::Value>; // 用来保存异步调用结果
            using JsonResponseCallback = std::function<void(const Json::Value&)>;
        private:
            Requestor::ptr _requestor;
        public:
            // Requestor由上层构建，因为它还需要用来初始化其他管理请求的类
            RpcCaller(const Requestor::ptr& requestor)
                : _requestor(requestor)
            {}

            // 这里采用异步调用来获取结果
            // 第一个参数是连接，第二个参数是方法名，第三个参数是方法的参数，第四个方法是future异步保存响应结果
            bool call(const BaseConnection::ptr& conn, const std::string& method, 
                      const Json::Value& params, JsonAsyncResponse& result)
            {
                // 需要向服务器发送异步回调请求，设置回调函数，回调函数中会传入一个promise对象，在回调函数中去让promise设置数据
                // 1. 组织请求
                auto req_msg = MessageFactory::create<RpcRequest>();
                req_msg->SetId(UUID::uuid());
                req_msg->SetMytype(MType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParms(params);
                LOG(DEBUG, "-----------1-----------\n");
                // ...
                auto json_promise = std::make_shared<std::promise<Json::Value>>();
                result = json_promise->get_future();    // 建立联系
                Requestor::RequestCallback cb = std::bind(&RpcCaller::Callback, this, json_promise, std::placeholders::_1);
                LOG(DEBUG, "-----------2-----------\n");
                // 2. 发送请求
                // 3. 等待响应
                bool ret = _requestor->send(conn, req_msg, cb); /////////////////////////////////////
                LOG(DEBUG, "-----------3-----------\n");
                if(ret == false)
                {
                    LOG(FATAL, "异步Rpc请求失败!\n");
                    return false;
                }
                return true;
            }

            // 采用同步调用来获取结果
            bool call(const BaseConnection::ptr& conn, const std::string& method, 
                      const Json::Value& params, Json::Value& result)
            {
                // 1. 组织请求
                auto req_msg = MessageFactory::create<RpcRequest>();
                req_msg->SetId(UUID::uuid());
                req_msg->SetMytype(MType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParms(params);

                BaseMessage::ptr rsp_msg;
                // 2. 发送请求
                bool ret = _requestor->send(conn, req_msg, rsp_msg);        
                if(ret == false)
                {
                    LOG(FATAL, "同步Rpc请求失败!\n");
                    return false;
                }
                std::cout << req_msg->serialize() << std::endl;
                LOG(DEBUG, "请求发送成功\n");
                // 3. 等待响应
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(rsp_msg);
                if (!rpc_rsp_msg) 
                {
                    LOG(WARING, "rpc响应，向下类型转换失败!\n");
                    return false;
                }
                if (rpc_rsp_msg->rcode() != RCode::RCODE_OK) 
                {
                    LOG(WARING, "rpc请求出错: %s\n", errReason(rpc_rsp_msg->rcode()).c_str());
                    return false;
                }
                result = rpc_rsp_msg->result();
                return true;
            }

            // 采用回调方法调用来获取结果
            bool call(const BaseConnection::ptr& conn, const std::string& method, 
                      const Json::Value& params, const JsonResponseCallback &cb)
            {
                // 1. 组织请求
                auto req_msg = MessageFactory::create<RpcRequest>();
                req_msg->SetId(UUID::uuid());
                req_msg->SetMytype(MType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParms(params);

                Requestor::RequestCallback req_cb = std::bind(&RpcCaller::CallbackA, this, cb, std::placeholders::_1);
                // 2. 发送请求
                // 3. 等待响应
                bool ret = _requestor->send(conn, req_msg, req_cb);
                if(ret == false)
                {
                    LOG(FATAL, "异步Rpc请求失败!\n");
                    return false;
                }
                return true;
            }

        private:
            void CallbackA(const JsonResponseCallback &cb, const BaseMessage::ptr &msg)
            {
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!rpc_rsp_msg) 
                {
                    LOG(WARING, "rpc响应，向下类型转换失败!\n");
                    return ;
                }
                if (rpc_rsp_msg->rcode() != RCode::RCODE_OK) 
                {
                    LOG(WARING, "rpc请求出错：%s", errReason(rpc_rsp_msg->rcode()));
                    return ;
                }
                // 直接处理响应的结果
                cb(rpc_rsp_msg->result());
            }

            void Callback(std::shared_ptr<std::promise<Json::Value>> result, const BaseMessage::ptr& msg)
            {
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
                if(!rpc_rsp_msg)
                {
                    LOG(FATAL, "rpc响应，向下类型转换失败!\n");
                    return;
                }
                if(rpc_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    LOG(WARING, "rpc异步请求出错：%s", errReason(rpc_rsp_msg->rcode()))
                }
                result->set_value(rpc_rsp_msg->result());
                LOG(DEBUG, "promise成功设置好值了\n");
            }
        };
    };
};