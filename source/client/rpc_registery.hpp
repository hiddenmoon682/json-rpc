#pragma once
#include "requestor.hpp"
#include <unordered_set>

namespace util_ns
{
    namespace client
    {
        // 客户端作为服务提供者和服务发现者是分离的，如果是提供者就不可能是发现者

        // 描述作为服务提供者的客户端
        class Provider
        {
        private:
            Requestor::ptr _requestor;  // 用于发送服务注册请求
        public:
            using ptr = std::shared_ptr<Provider>;
            
            Provider(const Requestor::ptr& requestor)
                : _requestor(requestor)
            {}

            bool regitryMethod(const BaseConnection::ptr& conn, const std::string& method, const Address& host)
            {
                // 1. 构建请求报文
                auto msg_req = MessageFactory::create<ServiceRequest>();
                msg_req->SetMytype(MType::REQ_SERVICE);
                msg_req->SetId(UUID::uuid());
                msg_req->setMethod(method);
                msg_req->setHost(host);
                msg_req->setOptype(ServiceOptype::SERVICE_REGISTRY);
                // 2. 发送请求，获取响应, 判断注册是否成功
                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if(ret == false)
                {
                    LOG(WARING, "服务注册失败\n");
                    return false;
                }
                auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
                if(service_rsp.get() == nullptr)
                {
                    LOG(WARING, "响应类型向下转换失败！\n");
                    return false;
                }
                if(service_rsp->rcode() != RCode::RCODE_OK)
                {
                    LOG(WARING, "服务注册失败，原因：%s\n", errReason(service_rsp->rcode()).c_str());
                    return false;
                }
                return true;
            }
        };

        // 用于服务发现者管理服务提供者主机地址的类
        class MethodHost
        {
        private:
            std::mutex _mutex;  // 保证线程安全
            size_t _idx;    // 用于RR轮转计数
            std::vector<Address> _hosts;    // 管理服务提供者主机地址
        public:
            using ptr = std::shared_ptr<MethodHost>;
            
            MethodHost(): _idx(0)
            {}

            MethodHost(const std::vector<Address>& hosts)
                : _hosts(hosts.begin(), hosts.end()), _idx(0)
            {}

            //  添加主机地址
            void appendHost(const Address& host)
            {
                // 中途收到了服务上线请求后被调用
                std::unique_lock<std::mutex> lock(_mutex);
                _hosts.push_back(host);
            }

            // 移除主机地址
            void removeHost(const Address& host)
            {
                // 中途收到了服务下线请求后被调用
                std::unique_lock<std::mutex> lock(_mutex);
                for(auto it = _hosts.begin(); it != _hosts.end(); ++it)
                {
                    if(*it == host)
                    {
                        _hosts.erase(it);
                        break;
                    }
                }
            }

            // 根据RR轮转，选择出当前请求的目标服务器主机地址
            Address chooseHost()
            {   
                std::unique_lock<std::mutex> lock(_mutex);
                size_t pos = _idx++ % _hosts.size();
                return _hosts[pos];
            }

            // 判断是否为空
            bool empty()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                return _hosts.empty();
            }
        };

        // 描述作为服务发现者的客户端
        class Discoverer
        {
        public:
            using ptr = std::shared_ptr<Discoverer>;
            using OfflineCallback = std::function<void(const Address&)>;   // 。。。。。。。。。。
        private:
            std::mutex _mutex;
            Discoverer::OfflineCallback _offline_callback;
            std::unordered_map<std::string, MethodHost::ptr> _method_hosts; // 服务方法和对应的服务提供主机地址的映射
            Requestor::ptr _requestor;      // 用于服务发现请求发送
        public:          
            Discoverer(const Requestor::ptr& requesotr, const OfflineCallback &cb)
                : _requestor(requesotr), _offline_callback(cb)
            {}

            // 服务发现调用
            // 第一个参数是和注册中心的连接，第二个参数是要发现的服务的名称，第三个参数是输出型参数，返回主机地址
            bool serviceDiscovery(const BaseConnection::ptr& conn, const std::string& method, Address& host)
            {
                // 先查找当前保管的提供者信息中是否存在对应的服务，若存在，则直接返回地址
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _method_hosts.find(method);
                    if(it != _method_hosts.end())
                    {
                        // 存在
                        host = it->second->chooseHost();
                        return true;
                    }
                }
                // 当前服务提供者为空，进行服务发现请求
                // 1. 构建请求
                auto msg_req = MessageFactory::create<ServiceRequest>();
                msg_req->SetMytype(MType::REQ_SERVICE);
                msg_req->SetId(UUID::uuid());
                msg_req->setMethod(method);
                msg_req->setOptype(ServiceOptype::SERVICE_DISCOVERY);
                // 2. 发送请求，等待响应
                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if(ret == false)
                {
                    LOG(WARING, "服务发现失败\n");
                    return false;
                }
                auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
                if(service_rsp.get() == nullptr)
                {
                    LOG(WARING, "响应类型向下转换失败！\n");
                    return false;
                }
                if(service_rsp->rcode() != RCode::RCODE_OK)
                {
                    LOG(WARING, "%s 服务发现失败！没有能够提供服务的主机！\n", method.c_str());
                    return false;
                }
                // 走到这里，说明该方法当前是没有对应的服务提供者主机的
                // 提取响应信息，更新_method_hosts映射关系
                auto method_host = std::make_shared<MethodHost>(service_rsp->hosts());
                if(method_host->empty())
                {
                    // 空的，说明没有提供该服务的服务提供者主机
                    LOG(INFO, "%s 服务发现失败！没有能够提供服务的主机！\n", method.c_str());
                    return false;
                }
                host = method_host->chooseHost();
                // 不使用insert，避免存在空的std::vector<MethodHost>导致插入失败
                std::unique_lock<std::mutex> lock(_mutex);
                _method_hosts[method] = method_host;
                return true;
            }

            // 这个接口是提供给Dispatcher模块进行服务上线下线请求处理的回调函数
            void onServiceRequest(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                // 1. 判断是上线请求还是下线请求，如果都不是就不处理了
                auto optype = msg->optype();
                std::string method = msg->method();
                
                std::unique_lock<std::mutex> lock(_mutex);
                if(optype == ServiceOptype::SERVICE_ONLINE)
                {
                    // 服务上线，找到对应服务的methodHost，向其中添加一个主机地址
                    auto it = _method_hosts.find(method);
                    if(it == _method_hosts.end())
                    {
                        // 没有对应的映射关系，需要新建MethodHost
                        auto method_host = std::make_shared<MethodHost>();
                        method_host->appendHost(msg->host());
                        _method_hosts[method] = method_host;
                    }
                    else
                    {
                        it->second->appendHost(msg->host());
                    }
                }
                else if(optype == ServiceOptype::SERVICE_OFFLINE)
                {
                    // 服务下线，不是主机下线，找到MethodHost并删掉其中一个主机地址即可
                    auto it = _method_hosts.find(method);
                    if(it == _method_hosts.end())
                    {
                        return;
                    }
                    it->second->removeHost(msg->host());
                    _offline_callback(msg->host());
                }
            }
        };
    };
};