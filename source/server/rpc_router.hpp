#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"

namespace util_ns
{
    // 避免和client相关类产生命名冲突，再来一层命名空间
    namespace server
    {
        // 用于检查rpc请求时传入的参数是否合理正确
        enum class VType
        {
            BOOL = 0,
            INTEGRAL,
            NUMERIC,
            STRING,
            ARRAY,
            OBJECT
        };

        // 对于一种服务的描述类
        class ServiceDescribe
        {
        public:
            using ptr = std::shared_ptr<ServiceDescribe>;
            using ParamsDescribe = std::pair<std::string, VType>;   // 参数名对应参数类型
            using ServiceCallback = std::function<void(const Json::Value&, Json::Value&)>;  // 实际的业务回调函数
        private:
            std::string _method_name;   // 方法名称
            ServiceCallback _callback;  // 实际的业务回调函数
            std::vector<ParamsDescribe> _params_desc;   // 参数字段格式描述
            VType _return_type;         // 返回值的类型
        public:
            ServiceDescribe(std::string&& mname, std::vector<ParamsDescribe>&& desc, VType vtype, ServiceCallback&& callback)
                : _method_name(std::move(mname)), 
                _params_desc(std::move(desc)),
                _return_type(vtype),
                _callback(std::move(callback))
            {}

            const std::string& method()
            {
                return _method_name;
            }

             //针对收到的请求中的参数进行校验
            bool paramCheck(const Json::Value &param)
            {
                //对params进行参数校验---判断所描述的参数字段是否存在，类型是否一致
                for(auto& desc : _params_desc)
                {
                    // 检验参数名称
                    if(param.isMember(desc.first) == false)
                    {
                        LOG(WARING, "参数字段完整性校验失败！%s 字段缺失!\n", desc.first.c_str());
                        return false;
                    }
                    if(check(desc.second, param[desc.first]) == false)
                    {
                        LOG(WARING, "%s 参数类型校验失败!\n", desc.first.c_str());
                        return false;
                    }
                }
                return true;
            }

            // 调用业务回调函数
            bool call(const Json::Value& params, Json::Value& result)
            {
                _callback(params, result);
                if(rtypeCheck(result) == false)
                {
                    LOG(WARING, "回调处理函数中的响应信息校验失败!\n");
                    return false;
                }
                return true;
            }

        private:
            // 判断返回值的类型是否正确
            bool rtypeCheck(const Json::Value& result)
            {
                return check(_return_type, result);
            }

            // 检验参数对应的类型是否正确
            // 第一给参数是函数规定的参数对应的类型，第二个参数是要传过来的参数要检验的对象
            bool check(VType vtype, const Json::Value& val)
            {
                switch (vtype)
                {
                    case VType::BOOL : return val.isBool(); 
                    case VType::INTEGRAL : return val.isIntegral(); 
                    case VType::NUMERIC : return val.isNumeric(); 
                    case VType::STRING : return val.isString(); 
                    case VType::ARRAY : return val.isArray(); 
                    case VType::OBJECT : return val.isObject(); 
                }
                return false;
            }
        };



        // ServiceDescribe类的工厂类，为了避免在ServiceDescribe类中提供直接设置成员变量的方法而造成风险
        // 使用建造模式的工厂类
        class SDescribeFactory
        {
        private:
            std::string _method_name;   
            ServiceDescribe::ServiceCallback _callback; 
            std::vector<ServiceDescribe::ParamsDescribe> _params_desc; 
            VType _return_type;
        public:
            using ptr = std::shared_ptr<SDescribeFactory>;

            // 设置字段
            void setMethodName(const std::string& name)
            {
                _method_name = name;
            }

            void setCallback(const ServiceDescribe::ServiceCallback& cb)
            {
                _callback = cb;
            }

            void setParamsDesc(const std::string &pname, VType vtype)
            {
                _params_desc.push_back(std::make_pair(pname, vtype));
            }

            void setReturnType(VType vtype)
            {
                _return_type = vtype;
            }

            ServiceDescribe::ptr build()
            {
                return std::make_shared<ServiceDescribe>(std::move(_method_name), std::move(_params_desc), _return_type, std::move(_callback));
            }
        };


        // 服务管理类，用于管理服务器能够替提供的服务
        class ServiceManager
        {
        public:
            using ptr = std::shared_ptr<ServiceManager>;

            // 查
            ServiceDescribe::ptr select(const std::string& method_name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _services.find(method_name);
                if(it == _services.end())
                {
                    return ServiceDescribe::ptr();
                }
                return it->second;
            }

            // 增
            void insert(const ServiceDescribe::ptr &desc)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _services.insert(std::make_pair(desc->method(), desc));
            }

            // 删
            void remove(const std::string& name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _services.erase(name);
            }
        private:
            std::mutex _mutex;      // 保证线程安全
            std::unordered_map<std::string, ServiceDescribe::ptr> _services;    // 服务名称和服务描述类的映射关系
        };

        // 为上层提供对应的接口
        class RpcRouter
        {
        private:
            ServiceManager::ptr _service_manager;
        public:
            using ptr = std::shared_ptr<RpcRouter>;

            RpcRouter():_service_manager(std::make_shared<ServiceManager>())
            {}

            //这是注册到Dispatcher模块针对rpc请求进行回调处理的业务函数
            void onRpcRequest(const BaseConnection::ptr &conn, RpcRequest::ptr &request)
            {
                //1. 查询客户端请求的方法描述--判断当前服务端能否提供对应的服务
                auto service = _service_manager->select(request->method());
                if(service.get() == nullptr)
                {
                    LOG(INFO, "%s 服务未找到!\n", request->method().c_str());
                    return response(conn, request, Json::Value(), RCode::RCODE_NOT_FOUND_SERVICE);
                }
                //2. 进行参数校验，确定能否提供服务
                if(service->paramCheck(request->parms()) == false)
                {
                    LOG(INFO, "%s 服务参数校验失败!\n", request->method().c_str());
                    return response(conn, request, Json::Value(), RCode::RCODE_INVALID_PARAMS);
                }
                //3. 调用业务回调接口进行业务处理
                Json::Value result;
                bool ret = service->call(request->parms(), result);
                if(ret == false)
                {
                    LOG(INFO, "计算结果返回值类型错误!\n");
                    return response(conn, request, Json::Value(), RCode::RCODE_INTERNAL_ERROR);
                }
                //4. 处理完毕得到结果，组织响应，向客户端发送
                return response(conn, request, result, RCode::RCODE_OK);
            }

            // 服务注册
             void registerMethod(const ServiceDescribe::ptr& service)
            {
                _service_manager->insert(service);
            }
        
        private:
            void response(const BaseConnection::ptr &conn, const RpcRequest::ptr &req, const Json::Value &res, RCode rcode)
            {
                auto msg = MessageFactory::create<RpcResponse>();
                msg->SetId(req->rid());
                msg->SetMytype(util_ns::MType::RSP_RPC);
                msg->setRCode(rcode);
                msg->setResult(res);
                conn->send(msg);
            }
        };

    };
};