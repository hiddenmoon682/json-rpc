#pragma once
#include "../common/dispatcher.hpp"
#include "../client/rpc_client.hpp"
#include "rpc_router.hpp"
#include "rpc_registry.hpp"
#include "rpc_topic.hpp"

namespace util_ns
{
    namespace server
    {
        // 注册中心服务端：只需要针对服务注册与发现请求进行处理即可
        class RegistryServer
        {
        private:
            PDManager::ptr _pd_manager; // 管理服务提供者Provider和服务发现者Discoverer
            Dispatcher::ptr _dispatcher;    // 管理数据包分发
            BaseServer::ptr _server;        // 服务器
        public:
            using ptr = std::shared_ptr<RegistryServer>;

            RegistryServer(int port)
                :_pd_manager(std::make_shared<PDManager>()),
                _dispatcher(std::make_shared<Dispatcher>())
            {
                auto service_cb = std::bind(&PDManager::onServiceRequest, _pd_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE, service_cb);

                _server = ServerFactory::create(port);
                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallback(message_cb);

                auto close_cb = std::bind(&RegistryServer::onConnShutdown, this, std::placeholders::_1);
                _server->setCloseCallback(close_cb);
            }

            void start()
            {
                _server->start();
            }

        private:
            // 有服务提供者或发现者断开连接，则进行对应的处理，比如通知
            void onConnShutdown(const BaseConnection::ptr& conn)
            {
                _pd_manager->onConnectionShutdown(conn);
            }
        };

        // Rpc服务服务器
        class RpcServer
        {
        private:
            bool _enableRegistry;                       // 确认是否使用注册中心
            Address _access_addr;
            client::ReigstryClient::ptr _reg_client;    // 注册中心客户端
            RpcRouter::ptr _router;                     // Rpc服务管理
            Dispatcher::ptr _dispatcher;                // 管理数据包分发
            BaseServer::ptr _server;                // 服务器
        public:
            using ptr = std::shared_ptr<RpcServer>;

            //rpc——server端有两套地址信息：
            //  1. rpc服务提供端地址信息--必须是rpc服务器对外访问地址（云服务器---监听地址和访问地址不同）
            //  2. 注册中心服务端地址信息 -- 启用服务注册后，连接注册中心进行服务注册用的
            RpcServer(const Address& access_addr, bool enableRegistry = false, const Address& registry_server_addr = Address())
                :_enableRegistry(enableRegistry),
                _access_addr(access_addr),
                _router(std::make_shared<RpcRouter>()),
                _dispatcher(std::make_shared<Dispatcher>())
            {
                if(enableRegistry)
                {
                    // 使用注册中心
                    _reg_client = std::make_shared<client::ReigstryClient>(registry_server_addr.first, registry_server_addr.second);
                }

                // 当前成员server是一个rpcserver，用于提供rpc服务的
                auto rpc_cb = std::bind(&RpcRouter::onRpcRequest, _router.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<RpcRequest>(MType::REQ_RPC, rpc_cb);

                _server = ServerFactory::create(access_addr.second);
                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallback(message_cb);
            }

            // 方法注册
            void registerMethod(const ServiceDescribe::ptr& service)
            {
                if(_enableRegistry)
                {
                    _reg_client->registryMethod(service->method(), _access_addr);
                }
                _router->registerMethod(service);
            }

            void start()
            {
                _server->start();
            }
        };
        
        // 主题服务器
        class TopicServer
        {
        private:
            TopicManager::ptr _topic_manager;   // 主题管理
            Dispatcher::ptr _dispatcher;
            BaseServer::ptr _server;
        public:
            using ptr = std::shared_ptr<TopicServer>;
            
            TopicServer(int port)
                :_topic_manager(std::make_shared<TopicManager>()),
                _dispatcher(std::make_shared<Dispatcher>())
            {
                auto topic_cb = std::bind(&TopicManager::onTopicRequest, _topic_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<TopicRequest>(MType::REQ_TOPIC, topic_cb);

                _server = ServerFactory::create(port);
                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallback(message_cb);

                // >>>>>>>>>>>>>>>>>>>>>
                auto close_cb = std::bind(&TopicServer::onConnShutdown, this, std::placeholders::_1);
                _server->setCloseCallback(close_cb);
            }

            void start()
            {
                _server->start();
            }

        private:
            void onConnShutdown(const BaseConnection::ptr& conn)
            {
                _topic_manager->onShutdown(conn);
            }
        };
    };
};
