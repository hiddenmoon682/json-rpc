#pragma once
#include "requestor.hpp"
#include <unordered_set>

namespace util_ns
{
    namespace client
    {
        // 管理主题信息
        class TopicManager
        {
        public:
            using ptr = std::shared_ptr<TopicManager>;
            using SubCallback = std::function<void(const std::string& key, const std::string& msg)>;    
        private:
            std::mutex _mutex;
            std::unordered_map<std::string, SubCallback> _topic_callbacks;   // 主题和处理对应推送的回调函数的映射
            Requestor::ptr _requestor;  // 管理发送请求
        public:
            TopicManager(const Requestor::ptr& requestor)
                :_requestor(requestor)
            {}

            // 请求创建主题
            bool create(const BaseConnection::ptr& conn, const std::string& key)
            {
                bool ret;
                ret = commonRequest(conn, key, TopicOptype::TOPIC_CREATE);
                if(ret)
                {
                    LOG(DEBUG, "主题创建成功\n");
                }
                else
                {
                    LOG(DEBUG, "主题创建失败\n");
                }
                return ret;
            }

            // 请求删除主题
            bool remove(const BaseConnection::ptr& conn, const std::string& key)
            {
                return commonRequest(conn, key, TopicOptype::TOPIC_REMOVE);
            }

            // 请求订阅主题
            bool subscribe(const BaseConnection::ptr& conn, const std::string &key, const SubCallback &cb)
            {
                addSubscribe(key, cb);
                bool ret = commonRequest(conn, key, TopicOptype::TOPIC_SUBSCRIBE);
                if(ret == false)
                {
                    delSubscribe(key);
                    LOG(DEBUG, "主题订阅失败\n");
                    return false;
                }
                LOG(DEBUG, "主题订阅成功\n");
                return true;
            }

            // 请求取消订阅主题
            bool cancel(const BaseConnection::ptr &conn, const std::string &key)
            {
                delSubscribe(key);
                return commonRequest(conn, key, TopicOptype::TOPIC_CANCEL);
            }

            // 根据主题推送对应的消息
            bool publish(const BaseConnection::ptr &conn, const std::string &key, const std::string &msg)
            {
                return commonRequest(conn, key, TopicOptype::TOPIC_PUBLISH, msg);
            }

            // 如果收到推送，则相应的处理
            void onPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr& msg)
            {
                // 1. 从消息中取出操作类型进行判断，是否是消息请求
                auto type = msg->optype();
                if(type != TopicOptype::TOPIC_PUBLISH)
                {
                    LOG(WARING, "收到了错误类型的主题操作！\n");
                    return;
                }
                // 2. 取出消息主题名称，以及消息内容
                std::string topic_key = msg->topicKey();
                std::string topic_msg = msg->topicMsg();
                // 3. 通过主题名称，查找对应主题的回调处理函数，有在处理，无在报错
                auto callback = getSubscribe(topic_key);
                if(!callback)
                {
                    LOG(WARING, "收到了 %s 主题消息，但是该消息无主题处理回调！\n", topic_key.c_str());
                    return;
                }
                return callback(topic_key, topic_msg);
            }
        private:
            // 添加订阅
            void addSubscribe(const std::string& key, const SubCallback& cb)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _topic_callbacks.insert(std::make_pair(key, cb));
            }

            // 删除订阅
            void delSubscribe(const std::string& key)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _topic_callbacks.erase(key);
            }

            // 获取订阅
            const SubCallback getSubscribe(const std::string& key)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _topic_callbacks.find(key);
                if(it == _topic_callbacks.end())
                {
                    return SubCallback();
                }
                return it->second;
            }

            // 发起对应的请求
            // key是主题名称，type是对应的主题操作，msg是消息，根据操作类型判断是否需要msg
            bool commonRequest(const BaseConnection::ptr& conn, const std::string& key, TopicOptype type, const std::string& msg = "")
            {
                // 1. 构造请求对象
                auto msg_req = MessageFactory::create<TopicRequest>();
                msg_req->SetMytype(MType::REQ_TOPIC);
                msg_req->SetId(UUID::uuid());
                msg_req->setOptype(type);
                msg_req->setTopicKey(key);
                if(type == TopicOptype::TOPIC_PUBLISH)
                {
                    msg_req->setTopicMsg(msg);
                }
                // 2. 向服务端发送请求，等待响应
                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if(ret == false)
                {
                    LOG(WARING, "主题操作请求失败！\n");
                    return false;
                }
                // 3. 判断请求处理是否成功
                auto topic_rsp_msg = std::dynamic_pointer_cast<TopicResponse>(msg_rsp);
                if(!topic_rsp_msg)
                {
                    LOG(WARING, "主题操作响应，向下类型转换失败！\n");
                    return false;
                }
                if(topic_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    LOG(WARING, "主题操作请求出错：%s\n", errReason(topic_rsp_msg->rcode()));
                    return false;
                }
                return true;
            }
        };
    };
};