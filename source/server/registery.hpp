#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <set>

namespace util_ns
{
    namespace server
    {
        // 服务注册中心同时需要对服务提供者和服务发现者进行相应的管理

        // 服务提供者管理
        class ProviderManager
        {
        public:
            using ptr = std::shared_ptr<ProviderManager>;
            // 能够提供服务的主机的描述
            struct Provider
            {
                using ptr = std::shared_ptr<Provider>;
                std::mutex _mutex;          // 保证methods线程安全
                BaseConnection::ptr conn;   // 相应的连接
                Address host;               // 主机地址
                std::vector<std::string> methods;   // 能够提供的方法

                Provider(const BaseConnection::ptr& c, const Address& h)
                    : conn(c), host(h)
                {}

                // 添加能够提供的服务
                void appendMethod(const std::string& method)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    methods.emplace_back(method);    // 插入方法
                }
            };   
        private:
            std::mutex _mutex;  // 保证线程安全
            std::unordered_map<std::string, std::set<Provider::ptr>> _providers;   // 服务方法名称和能够提供该服务的服务提供者的映射
            std::unordered_map<BaseConnection::ptr, Provider::ptr> _conns;  // 连接和主机的映射
        public:
            // 增：当一个新的服务提供者进行服务注册的时候调用
            void addProvider(const BaseConnection::ptr& c, const Address& h, const std::string& method)
            {   
                Provider::ptr provider;
                {
                    // 先查找连接所关联的服务提供者对象是否存在，找到则获取，找不到则创建，并建立关联
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if(it != _conns.end())
                    {
                        provider = it->second;
                    }
                    // 没找到则创建一个，然后插入_conns中
                    else
                    {
                        provider = std::make_shared<Provider>(c, h);
                        _conns.insert(std::make_pair(c, provider));
                    }
                    // method方法的提供主机要多出一个, 先获取set，然后插入, _providers新增数据
                    auto& providers = _providers[method];
                    providers.insert(provider);
                }
                // 服务对象provider中页也要新增对应的服务名称
                provider->appendMethod(method);
            }

            // 查: 当一个服务提供者断开连接的时候，获取他的信息--用于进行服务下线通知
            Provider::ptr getProvider(const BaseConnection::ptr& c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if(it == _conns.end())
                {
                    return Provider::ptr();
                }
                return it->second;
            }

            // 删：当一个服务提供者断开连接的时候，删除它的关联信息
            void delProvider(const BaseConnection::ptr& c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if(it == _conns.end())
                {
                    // 没找到，说明该连接不是服务提供者
                    return ;
                }
                // 找到了，如果是提供者，看看提供了什么服务，从服务者提供信息中删除当前服务提供者
                for(std::string& method : it->second->methods)
                {
                    std::set<Provider::ptr>& providers = _providers[method];
                    providers.erase(it->second);
                }
                // 删除连接与服务提供者的映射关系
                _conns.erase(it);
            }

            // 获取当前服务都有哪些提供者，返回对应的地址
            std::vector<Address> methodHosts(const std::string& method)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _providers.find(method);
                if(it == _providers.end())
                {
                    // 没找到，说明没有该服务
                    return std::vector<Address>();
                }
                std::vector<Address> result;
                for(auto& provider : it->second)
                {
                    result.push_back(provider->host);
                }
                return result;
            }
        };
        
        // 服务发现者管理
        class DiscovererManager
        {
        public:
            using ptr = std::shared_ptr<DiscovererManager>;
            // 进行服务发现的主机的描述
            struct Discoverer
            {
                using ptr = std::shared_ptr<Discoverer>;
                std::mutex _mutex;          // 保证methods线程安全
                BaseConnection::ptr conn;   // 发现者关联的客户端连接
                std::vector<std::string> methods;   // 发现过的服务的名称

                Discoverer(const BaseConnection::ptr& c)
                    : conn(c)
                {}

                // 添加发现过的服务
                void appendMethod(const std::string& method)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    methods.emplace_back(method);    // 插入方法
                }
            };   
        private:
            std::mutex _mutex;  // 保证线程安全
            std::unordered_map<std::string, std::set<Discoverer::ptr>> _discoverers;   // 服务方法名称和发现过该服务的服务发现者的映射
            std::unordered_map<BaseConnection::ptr, Discoverer::ptr> _conns;  // 连接和服务发现主机的映射
        public:
            // 当每次客户端进行服务发现的时候新增发现者，新增服务名称
            Discoverer::ptr addDiscoverer(const BaseConnection::ptr& c, const std::string& method)
            {
                Discoverer::ptr discoverer;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    // 查找该服务发现者是否已经存在，如果不存在就新增一个，并且插入_conns管理
                    auto it = _conns.find(c);
                    if(it != _conns.end())
                    {
                        discoverer = it->second;
                    }
                    else
                    {
                        discoverer = std::make_shared<Discoverer>(c);
                        _conns.insert(std::make_pair(c, discoverer));
                    }
                    auto& discoverers = _discoverers[method];
                    discoverers.insert(discoverer);
                }
                discoverer->appendMethod(method);
                return discoverer;
            }

            // 删：当一个服务发现者断开连接的时候，删除它的关联信息
            void delDiscoverer(const BaseConnection::ptr& c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if(it == _conns.end())
                {
                    // 没找到，说明该连接不是服务发现者
                    return ;
                }
                // 找到了，如果是发现者，看看发现过什么服务，从这些服务中删除这些服务发现者
                for(std::string& method : it->second->methods)
                {
                    std::set<Discoverer::ptr>& discoverers = _discoverers[method];
                    discoverers.erase(it->second);
                }
                // 删除连接与服务发现者的映射关系
                _conns.erase(it);
            }

            // 当有一个新的服务提供者上线，则进行上线通知
            void onlineNotify(const std::string& method, const Address& host)
            {
                return notify(method, host, ServiceOptype::SERVICE_ONLINE);
            }

            // 当有一个服务提供者下线，则进行下线通知
            void offlineNotify(const std::string& method, const Address& host)
            {
                return notify(method, host, ServiceOptype::SERVICE_OFFLINE);
            }

        private:
            // 通知服务发现者哪些服务的服务提供者上线或下线
            // 比如通知发现过Add服务的发现者现在又多了一台服务提供者，通知发现过Add服务的发现者少了一台服务提供者
            void notify(const std::string& method, const Address& host, ServiceOptype optype)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _discoverers.find(method);
                if(it == _discoverers.end())
                {
                    // 没找到，说明没有发现者尝试发现该服务
                    return ;
                }
                // 有发现者
                // 构建通知报文
                auto msg_req = MessageFactory::create<ServiceRequest>();
                msg_req->SetMytype(MType::REQ_SERVICE);
                msg_req->SetId(UUID::uuid());
                msg_req->setMethod(method);
                msg_req->setHost(host);
                msg_req->setOptype(optype);
                // 逐个发送通知
                for(auto& discoverer : it->second)
                {
                    discoverer->conn->send(msg_req);
                }
            }
        };

        // 管理服务提供者Provider和服务发现者Discoverer
        class PDManager
        {      
        private:
            ProviderManager::ptr _providers;        // 服务提供主机管理
            DiscovererManager::ptr _discoverers;    // 服务发现主机管理
        public:
            using ptr = std::shared_ptr<PDManager>;
            
            PDManager()
                : _providers(std::make_shared<ProviderManager>()),
                _discoverers(std::make_shared<DiscovererManager>())
            {}

            // 要注册进Dispathcher的针对ServiceRequestd请求的回调函数
            void onServiceRequest(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                // 两种服务操作请求：服务注册/服务发现
                ServiceOptype optype = msg->optype();
                // 服务注册
                if(optype == ServiceOptype::SERVICE_REGISTRY)
                {
                    LOG(INFO, "%s:%d 注册服务 %s\n", msg->host().first.c_str(), msg->host().second, msg->method().c_str());
                    // 1. 新增服务提供者
                    _providers->addProvider(conn, msg->host(), msg->method());
                    // 2. 进行服务上线通知
                    _discoverers->onlineNotify(msg->method(), msg->host());
                    // 3. 给服务提供者发送响应
                    return registeryResponse(conn, msg);
                }
                // 服务发现
                else if(optype == ServiceOptype::SERVICE_DISCOVERY)
                {
                    LOG(INFO, "客户端要进行 %s 服务发现！", msg->method().c_str());
                    // 1. 新增服务发现者
                    _discoverers->addDiscoverer(conn, msg->method());
                    // 2. 给服务发现者发送响应
                    return discoveryResponse(conn, msg);
                }
                // 异常
                else
                {
                    LOG(WARING, "收到服务操作请求，但是操作类型错误！");
                    errorResponse(conn, msg);
                }
            }

            // 服务提供者或服务发现者连接断开时的回调
            void onConnectionShutdown(const BaseConnection::ptr &conn)
            {
                // 1. 删除服务提供者相关信息
                // 2. 进行服务下线通知
                auto provider = _providers->getProvider(conn);
                if(provider.get() != nullptr)
                {
                    // 说明断开的连接是服务提供者
                    // 服务下线
                    for(auto& method : provider->methods)
                    {
                        _discoverers->offlineNotify(method, provider->host);
                    }
                    // 删除服务提供者相关信息
                    _providers->delProvider(conn);
                }
                // 如果是服务发现者断开连接，直接删除相关信息即可，没有通知等操作
                // 即使不是，调用函数也没有影响
                _discoverers->delDiscoverer(conn);
            }
        private:
            // 对服务提供者的响应
            void registeryResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                // 构建响应报文
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->SetMytype(MType::RSP_SERVICE);
                msg_rsp->SetId(msg->rid());
                msg_rsp->setRCode(RCode::RCODE_OK);
                msg_rsp->setOptype(ServiceOptype::SERVICE_REGISTRY);
                conn->send(msg_rsp);
            }

            // 对服务发现者的响应
            void discoveryResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                // 如果服务存在则返回对应的主机地址，如果不存在则异常返回
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->SetMytype(MType::RSP_SERVICE);
                msg_rsp->SetId(msg->rid());
                msg_rsp->setOptype(ServiceOptype::SERVICE_DISCOVERY);
                std::vector<Address> hosts = _providers->methodHosts(msg->method());
                if(hosts.empty())
                {
                    // 服务不存在
                    msg_rsp->setRCode(RCode::RCODE_NOT_FOUND_SERVICE);
                    return conn->send(msg_rsp);
                }   
                // 服务存在
                msg_rsp->setRCode(RCode::RCODE_OK);
                msg_rsp->setMethod(msg->method());
                msg_rsp->setHost(hosts);
                return conn->send(msg_rsp);
            }

            // 针对异常请求的响应
            void errorResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                auto msg_rsp = MessageFactory::create<ServiceResponse>();
                msg_rsp->SetMytype(MType::RSP_SERVICE);
                msg_rsp->SetId(msg->rid());
                msg_rsp->setRCode(RCode::RCODE_INVALID_OPTYPE);
                msg_rsp->setOptype(ServiceOptype::SERVICE_UNKNOW);
                conn->send(msg_rsp);
            }
        };
    }
}