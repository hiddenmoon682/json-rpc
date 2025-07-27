#pragma once

#include "abstract.hpp"
#include "fields.hpp"
#include "detail.hpp"

namespace util_ns
{
    class JsonMessage : public BaseMessage
    {
        // 使用protected保证继承的类也可以访问
    protected:
        Json::Value _body;

    public:
        using ptr = std::shared_ptr<JsonMessage>;

        // 序列化
        virtual std::string serialize() override
        {
            std::string body;
            auto ret = JSON::Serialize(_body, body);
            if (ret == false)
            {
                return nullptr;
            }
            return body;
        }
        // 反序列化
        virtual bool unserialize(const std::string &msg)
        {
            return JSON::UnSerialize(msg, _body);
        }
        // 反序列化后对信息进行校验
        virtual bool check() = 0;
    };

    // 请求基类
    class JsonRequest : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonRequest>;
    };

    // 响应基类
    class JsonResponse : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonResponse>;

        virtual bool check() override
        {
            // 在响应中，大部分响应都有并只有响应状态码
            // 因此只需要判断响应状态码是否存在，类型是否正确等
            // 真正判断响应码的内容是交给上层做的
            if (_body[KEY_RCODE].isNull() == true)
            {
                LOG(FATAL, "响应中没有响应码\n");
                return false;
            }
            if (_body[KEY_RCODE].isIntegral() == false)
            {
                LOG(FATAL, "响应码状态有误\n");
                return false;
            }
            return true;
        }

        virtual RCode rcode() 
        {
            return (RCode)_body[KEY_RCODE].asInt();
        }

        virtual void setRCode(RCode rcode) 
        {
            _body[KEY_RCODE] = (int)rcode;
        }
    };

    // Rpc请求
    class RpcRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<RpcRequest>;

        virtual bool check() override
        {
            // rpc请求，需要确认是否存在方法字段和参数字段及其格式
            if (_body[KEY_METHOD].isNull() == true ||
                _body[KEY_METHOD].isString() == false)
            {
                LOG(FATAL, "RPC请求中没有方法名称或方法名称类型错误!\n");
                return false;
            }
            if (_body[KEY_PARAMS].isNull() == true ||
                _body[KEY_PARAMS].isObject() == false)
            {
                LOG(FATAL, "RPC请求中没有参数信息或参数信息类型错误!\n")
                return false;
            }
            return true;
        }

        // 获取方法名
        std::string method()
        {
            return _body[KEY_METHOD].asString();
        }

        // 设置方法名
        void setMethod(const std::string &method_name)
        {
            _body[KEY_METHOD] = method_name;
        }

        // 获取方法参数
        Json::Value parms()
        {
            return _body[KEY_PARAMS];
        }

        // 设置方法参数
        void setParms(const Json::Value &parms)
        {
            _body[KEY_PARAMS] = parms;
        }
    };

    // 主题模块请求
    class TopicRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<TopicRequest>;

        virtual bool check() override
        {
            // topic请求，需要确认是否存在主题名称、消息内容及操作方法和格式是否正确
            // 以及在发布主题时消息内容是否为空
            if (_body[KEY_TOPIC_KEY].isNull() == true ||
                _body[KEY_TOPIC_KEY].isString() == false)
            {
                LOG(FATAL, "主题请求中没有主题名称或主题名称类型错误!\n");
                return false;
            }
            if (_body[KEY_OPTYPE].isNull() ||
                _body[KEY_OPTYPE].isIntegral() == false)
            {
                LOG(FATAL, "主题请求中没有操作类型或操作类型的类型错误!\n")
                return false;
            }
            if (_body[KEY_OPTYPE].asInt() == (int)TopicOptype::TOPIC_PUBLISH &&
                (_body[KEY_TOPIC_MSG].isNull() == true ||
                 _body[KEY_TOPIC_MSG].isString() == false))
            {
                LOG(FATAL, "主题消息发布请求中没有消息内容字段或消息内容类型错误!\n")
            }
            return true;
        }

        // 获取主题名称
        std::string topicKey()
        {
            return _body[KEY_TOPIC_KEY].asString();
        }

        // 设置主题名称
        void setTopicKey(const std::string &key)
        {
            _body[KEY_TOPIC_KEY] = key;
        }

        // 获取操作方法
        TopicOptype optype()
        {
            return (TopicOptype)_body[KEY_OPTYPE].asInt();
        }

        // 设置操作方法
        void setOptype(TopicOptype optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        // 获取消息内容名称
        std::string topicMsg()
        {
            return _body[KEY_TOPIC_MSG].asString();
        }

        // 设置消息内容
        void setTopicMsg(const std::string &msg)
        {
            _body[KEY_TOPIC_MSG] = msg;
        }
    };

    typedef std::pair<std::string, int> Address;
    // 申请服务请求
    class ServiceRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<ServiceRequest>;

        virtual bool check() override
        {
            // service请求，需要确认是否存在方法名称及格式等
            if (_body[KEY_METHOD].isNull() == true ||
                _body[KEY_METHOD].isString() == false)
            {
                LOG(FATAL, "服务请求中没有方法名称或方法名称类型错误!\n");
                return false;
            }
            if (_body[KEY_OPTYPE].isNull() ||
                _body[KEY_OPTYPE].isIntegral() == false)
            {
                LOG(FATAL, "服务请求中没有操作类型或操作类型的类型错误!\n")
                return false;
            }
            // 判断发送请求的主机及填写的格式是否正确
            // 在发起服务发现时不需要传递自身的主机地址？
            if (_body[KEY_OPTYPE].asInt() != (int)(ServiceOptype::SERVICE_DISCOVERY) &&
                (_body[KEY_HOST].isNull() == true ||
                 _body[KEY_HOST].isObject() == false ||
                 _body[KEY_HOST][KEY_HOST_IP].isNull() == true ||
                 _body[KEY_HOST][KEY_HOST_IP].isString() == false ||
                 _body[KEY_HOST][KEY_HOST_PORT].isNull() == true ||
                 _body[KEY_HOST][KEY_HOST_PORT].isIntegral() == false))
            {
                LOG(FATAL, "服务请求中主机地址信息错误!\n")
            }
            return true;
        }

        // 获取方法名称
        std::string method()
        {
            return _body[KEY_METHOD].asString();
        }

        // 设置方法名称
        void setMethod(const std::string &method_name)
        {
            _body[KEY_METHOD] = method_name;
        }

        // 获取操作方法
        ServiceOptype optype()
        {
            return (ServiceOptype)_body[KEY_OPTYPE].asInt();
        }

        // 设置操作方法
        void setOptype(ServiceOptype optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        // 获取主机信息
        Address host()
        {
            Address addr;
            addr.first = _body[KEY_HOST][KEY_HOST_IP].asString();
            addr.second = _body[KEY_HOST][KEY_HOST_PORT].asInt();
            return addr;
        }

        // 设置主机信息
        void setHost(const Address &host)
        {
            Json::Value val;
            val[KEY_HOST_IP] = host.first;
            val[KEY_HOST_PORT] = host.second;
            _body[KEY_HOST] = val;
        }
    };

    // Rpc响应
    class RpcResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<RpcResponse>;

        virtual bool check() override
        {
            // 检查rcode和result是否合规
            if (_body[KEY_RCODE].isNull() == true ||
                _body[KEY_RCODE].isIntegral() == false)
            {
                LOG(FATAL, "响应中没有响应状态码,或状态码类型错误!\n");
                return false;
            }
            if (_body[KEY_RESULT].isNull() == true)
            {
                LOG(FATAL, "响应中没有Rpc调用结果,或结果类型错误！!\n")
                return false;
            }
            return true;
        }

        // 获取响应结果
        Json::Value result()
        {
            return _body[KEY_RESULT];
        }

        // 设置响应结果
        void setResult(const Json::Value &result)
        {
            _body[KEY_RESULT] = result;
        }
    };

    // Topic响应
    class TopicResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<TopicResponse>;
    };

    // Service响应
    class ServiceResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<ServiceResponse>;

        virtual bool check() override
        {
            // 检查rcode和result是否合规
            if (_body[KEY_RCODE].isNull() == true ||
                _body[KEY_RCODE].isIntegral() == false)
            {
                LOG(FATAL, "响应中没有响应状态码,或状态码类型错误!\n");
                return false;
            }
            if(_body[KEY_OPTYPE].isNull() == true ||
               _body[KEY_OPTYPE].isIntegral() == false)
            {
                LOG(FATAL, "响应中没有操作类型,或操作类型的类型错误!\n");
            }
            // serviceresponse如果是对服务上线、注册等请求的响应，则应该没有方法相关的内容
            // 如果是服务发现，那么就需要返回对应的方法名称和参数，需要进行特殊检查
            if(_body[KEY_OPTYPE].asInt() == (int)(ServiceOptype::SERVICE_DISCOVERY) &&
              (_body[KEY_METHOD].isNull() == true ||
               _body[KEY_METHOD].isString() == false || 
               _body[KEY_HOST].isNull() == true ||
               _body[KEY_HOST].isArray() == false))
            {
                LOG(FATAL, "服务发现响应中响应信息字段错误!\n");
                return false;
            }
            return true;
        }

        // 获取optype推测响应类型
        ServiceOptype optype()
        {
            return (ServiceOptype)_body[KEY_OPTYPE].asInt();
        }

        // 设置optype
        void setOptype(ServiceOptype optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        std::string method() 
        {
            return _body[KEY_METHOD].asString();
        }

        void setMethod(const std::string &method)
        {
            _body[KEY_METHOD] = method;
        }

        // 设置能够提供服务的主机的地址,告诉客户端能够提供服务的都有哪些主机
        void setHost(std::vector<Address> addrs)
        {
            for(auto& addr : addrs)
            {
                Json::Value val;
                val[KEY_HOST_IP] = addr.first;
                val[KEY_HOST_PORT] = addr.second;
                _body[KEY_HOST].append(val);
            }
        }

        // 获取主机地址
        std::vector<Address> hosts()
        {
            std::vector<Address> addrs;
            int sz = _body[KEY_HOST].size();
            for(int i = 0; i < sz; i++)
            {
                Address addr;
                addr.first = _body[KEY_HOST][i][KEY_HOST_IP].asString();
                addr.second = _body[KEY_HOST][i][KEY_HOST_PORT].asInt();
                addrs.push_back(addr);
            }
            return addrs;
        }
    };

    // 实现一个消息对象的生产工厂
    class MessageFactory 
    {
    public:
        static BaseMessage::ptr create(MType mtype)
        {
            switch (mtype)
            {
                case MType::REQ_RPC : return std::make_shared<RpcRequest>();
                case MType::RSP_RPC : return std::make_shared<RpcResponse>();
                case MType::REQ_TOPIC : return std::make_shared<TopicRequest>();
                case MType::RSP_TOPIC : return std::make_shared<TopicResponse>();
                case MType::REQ_SERVICE : return std::make_shared<ServiceRequest>();
                case MType::RSP_SERVICE : return std::make_shared<ServiceResponse>();
            }
            return BaseMessage::ptr();
        }

        template<typename T, typename ...Args>
        static std::shared_ptr<T> create(Args&& ...args) 
        {
            return std::make_shared<T>(std::forward(args)...);
        }
    };
};
