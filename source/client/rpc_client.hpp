#pragma once
#include "../common/dispatcher.hpp"
#include "requestor.hpp"
#include "rpc_caller.hpp"
#include "rpc_registry.hpp"
#include "rpc_topic.hpp"

namespace util_ns
{
    namespace client
    {
        // 封装作为注册中心客户端
        class ReigstryClient
        {
        private:
            Requestor::ptr _requestor;          // 管理请求发送
            client::Provider::ptr _provider;    // 描述作为服务提供者的属性
            Dispatcher::ptr _dispatcher;        // 设置响应方法
            BaseClient::ptr _client;            // 连接
        public:
            using ptr = std::shared_ptr<ReigstryClient>;

            // 构造函数传入注册中心的地址信息，用于连接注册中心
            ReigstryClient(const std::string& ip, int port)
                :_requestor(std::make_shared<Requestor>()),
                _provider(std::make_shared<client::Provider>(_requestor)),
                _dispatcher(std::make_shared<Dispatcher>())
            {
                auto rsp_cb = std::bind(&Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MType::RSP_SERVICE, rsp_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client = ClientFactory::create(ip, port);
                _client->setMessageCallback(message_cb);
                _client->connect();
            }

            // 向外提供注册接口
            bool registryMethod(const std::string& method, const Address& host)
            {
                return _provider->regitryMethod(_client->connection(), method, host);
            }
        };

        // 封装作为服务发现者客户都
        class DiscoveryClient
        {
        private:
            Requestor::ptr _requestor;              // 管理请求发送
            client::Discoverer::ptr _discoverer;    // 描述作为服务发现者的属性
            Dispatcher::ptr _dispatcher;            // 设置响应方法
            BaseClient::ptr _client;                // 连接
        public:
            using ptr = std::shared_ptr<DiscoveryClient>;

            // 构造函数传入注册中心的地址信息，用于连接注册中心
            DiscoveryClient(const std::string& ip, int port, const Discoverer::OfflineCallback& cb)
                :_requestor(std::make_shared<Requestor>()),
                _discoverer(std::make_shared<client::Discoverer>(_requestor, cb)),
                _dispatcher(std::make_shared<Dispatcher>())
            {
                // 设置收到响应的回调函数
                auto rsp_cb = std::bind(&Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MType::RSP_SERVICE, rsp_cb);

                // 设置收到服务上下线请求的回调函数
                auto req_cb = std::bind(&Discoverer::onServiceRequest, _discoverer.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<ServiceRequest>(MType::REQ_SERVICE, req_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client = ClientFactory::create(ip, port);
                _client->setMessageCallback(message_cb);
                _client->connect();
            }

            // 向外提供的服务发现接口
            bool serviceDiscovery(const std::string& method, Address& host)
            {
                return _discoverer->serviceDiscovery(_client->connection(), method, host);
            }
        };

        // 描述作为发起Rpc请求的客户端
        // 作为Rpc客户端，它有两种形式一种是只作为rpc客户端，一种是能够服务发现的客户端，通过设置 _enableDiscovery 来确认它是哪种客户端

        // Rpc客户端在发起请求时也有两种方式，一种是长连接方式，也就是使用连接池，如果客户都向一个服务器发起请求，那么就保存与这个客户端的连接
        // 将这个连接放入连接池中。根据RR轮转如果下次请求连接另一个服务器，那么就再创建一个新连接，放入连接池
        // 如果下次又向一个请求过的服务器发起请求，就可以直接从连接池中取出连接发起请求，不用再次创建连接
        // 还有一种是短连接方式，也就是发起一次请求，就向服务器建立连接，请求完毕之后就删除连接，不过这种方式难以实现异步，
        // 因为可能还没有使用得到的异步结果future，连接就断开了，导致promise失效，future也无法使用了。连接断开是回调函数设置的，无法控制
        // 因此在这里使用长连接
        class RpcClient
        {
        private:
            // 因为Address是自定义类型，没有对应的hash函数，所以要自定义hash函数，这里使用了string的hash函数来生成
            struct AddressHash
            {
                size_t operator()(const Address& host) const 
                {
                    std::string addr = host.first + std::to_string(host.second);
                    return std::hash<std::string>{}(addr);
                }
            };
        private:
            std::mutex _mutex;                      // 线程安全
            bool _enableDiscovery;                  // 控制客户端类型
            Requestor::ptr _requestor;              // 管理请求发送
            Dispatcher::ptr _dispatcher;            // 管理回调函数
            RpcCaller::ptr _caller;                 // 用于发起rpc请求
            BaseClient::ptr _rpc_client;            // 未启用服务发现时与服务器之间的连接
            DiscoveryClient::ptr _discovery_client; // 服务发现客户端
            std::unordered_map<Address, BaseClient::ptr, AddressHash> _rpc_clients;   // 连接池
        public:
            using ptr = std::shared_ptr<RpcClient>;
            // enableDiscovery--是否启用服务发现功能，也决定了传入的地址信息是注册中心的地址，还是服务提供者的地址
            RpcClient(bool enableDiscovery, const std::string& ip, int port)
                :_enableDiscovery(enableDiscovery),
                _requestor(std::make_shared<Requestor>()),
                _dispatcher(std::make_shared<Dispatcher>()),
                _caller(std::make_shared<RpcCaller>(_requestor))
            {
                // 针对rpc请求后的响应进行的回调处理
                auto rsp_cb = std::bind(&Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MType::RSP_RPC, rsp_cb);

                // 如果启用了服务发现，地址信息是注册中心的地址，是服务发现客户端需要连接的地址，则通过地址信息实例化discovery_client
                // 如果没有启用服务发现，则地址信息是服务提供者的地址，则直接实例化好rpc_client
                if(_enableDiscovery)
                {
                    // 启用了
                    // 这里需要设置服务提供者连接断开时，对应的连接的回调，因为它们需要断开连接，删除对应的信息
                    auto offline_cb = std::bind(&RpcClient::delClient, this, std::placeholders::_1);
                    _discovery_client = std::make_shared<DiscoveryClient>(ip, port, offline_cb);
                }
                else
                {
                    // 没启用
                    auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                    _rpc_client = ClientFactory::create(ip, port);
                    _rpc_client->setMessageCallback(message_cb);
                    _rpc_client->connect();
                }
            }

            // 三种发起Rpc请求的方式
            // 同步
            bool call(const std::string& method, const Json::Value& params, Json::Value& result)
            {
                // 获取服务提供者，1.服务发现 2. 固定服务提供者、
                BaseClient::ptr client = getClient(method);
                if(client.get() == nullptr)
                {
                    LOG(DEBUG, "获取服务提供者失败\n");
                    return false;
                }
                return _caller->call(client->connection(), method, params, result);
            }

            // 异步
            bool call(const std::string& method, const Json::Value& params, RpcCaller::JsonAsyncResponse& result)
            {
                // 获取服务提供者，1.服务发现 2. 固定服务提供者、
                BaseClient::ptr client = getClient(method);
                if(client.get() == nullptr)
                {
                    LOG(DEBUG, "获取服务提供者失败\n");
                    return false;
                }
                return _caller->call(client->connection(), method, params, result);
            }

            // 回调
            bool call(const std::string& method, const Json::Value& params, const RpcCaller::JsonResponseCallback& cb)
            {
                // 获取服务提供者，1.服务发现 2. 固定服务提供者、
                BaseClient::ptr client = getClient(method);
                if(client.get() == nullptr)
                {
                    LOG(DEBUG, "获取服务提供者失败\n");
                    return false;
                }
                return _caller->call(client->connection(), method, params, cb);
            }
        private:
            // 删：连接断开时删除连接
            void delClient(const Address& host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _rpc_clients.erase(host);
            }

            // 有服务发现的情况下，通过host从连接池获取连接
            BaseClient::ptr getClient(const Address& host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _rpc_clients.find(host);
                if(it == _rpc_clients.end())
                {
                    return BaseClient::ptr();
                }
                return it->second;
            }

            // 查：通过method获取连接，服务发现可能有，也可能没有
            BaseClient::ptr getClient(const std::string& method)
            {
                BaseClient::ptr client;
                if(_enableDiscovery)
                {
                    // 1.通过服务发现，获取服务提供者的地址信息
                    Address host;
                    bool ret = _discovery_client->serviceDiscovery(method, host);
                    if(ret == false)
                    {
                        LOG(WARING, "当前 %s 服务，没有找到服务提供者！\n", method.c_str());
                        return BaseClient::ptr();
                    }
                    // 查看对应的服务提供者是否已有实例化客户端存储在_rpc_clients中，有则直接使用，没有则创建
                    client = getClient(host);
                    if(client.get() == nullptr)
                    {
                        client = newClient(host);
                    }
                }
                else
                {
                    client = _rpc_client;
                }
                return client;
            }

            // 增：新建一个连接添加进连接池并返回
            BaseClient::ptr newClient(const Address& host)
            {
                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                auto client = ClientFactory::create(host.first, host.second);
                client->setMessageCallback(message_cb);
                client->connect();
                putClient(host, client);
                return client;
            }

            // 增：添加进连接池
            void putClient(const Address& host, BaseClient::ptr& client)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _rpc_clients.insert(std::make_pair(host, client));
            }
        };

        // 主题客户端
        class TopicClient
        {
        private:
            Requestor::ptr _requestor;      // 请求发送管理
            Dispatcher::ptr _dispatcher;    // 报文分发管理
            TopicManager::ptr _topic_manager;   // 主题管理
            BaseClient::ptr _rpc_client;    // 客户端
        public:
            using ptr = std::shared_ptr<TopicClient>;
            
            TopicClient(const std::string& ip, int port)
                :_requestor(std::make_shared<Requestor>()),
                _dispatcher(std::make_shared<Dispatcher>()),
                _topic_manager(std::make_shared<TopicManager>(_requestor))
            {
                auto rsp_cb = std::bind(&Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MType::RSP_TOPIC , rsp_cb);

                auto msg_cb = std::bind(&TopicManager::onPublish , _topic_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<TopicRequest>(MType::REQ_TOPIC , msg_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage , _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _rpc_client = ClientFactory::create(ip, port);
                _rpc_client->setMessageCallback(message_cb);
                _rpc_client->connect();
            }

            // 创建主题
            bool create(const std::string& key)
            {
                return _topic_manager->create(_rpc_client->connection(), key);
            }

            // 删除主题
            bool remove(const std::string& key)
            {
                return _topic_manager->remove(_rpc_client->connection(), key);
            }

            // 订阅主题
            bool subscribe(const std::string& key, const TopicManager::SubCallback& cb)
            {
                return _topic_manager->subscribe(_rpc_client->connection(), key, cb);
            }

            // 取消订阅主题
            bool cancel(const std::string& key)
            {
                return _topic_manager->cancel(_rpc_client->connection(), key);
            }

            // 消息推送
            bool publish(const std::string& key, const std::string& msg)
            {
                return _topic_manager->publish(_rpc_client->connection(), key, msg);
            }

            // 关闭连接
            void shutdown()
            {
                _rpc_client->shutdown();
            }
        };
    };
};