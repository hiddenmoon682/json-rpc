#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <unordered_set>

namespace util_ns
{
    namespace server
    {   
        // 主题管理
        class TopicManager
        {
        private:
            // 订阅者描述类
            struct Subscriber
            {
                std::mutex _mutex;
                BaseConnection::ptr conn;               // 对应的连接
                std::unordered_set<std::string> topics; // 订阅者订阅的主题名称

                using ptr = std::shared_ptr<Subscriber>;

                Subscriber(const BaseConnection::ptr& c)
                    :conn(c)
                {}

                // 订阅主题的时候调用
                void appendTopic(const std::string& topic_name)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    topics.insert(topic_name);
                }

                // 主题被删除或者取消订阅时调用
                void removeTopic(const std::string& topic_name)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    topics.erase(topic_name);
                }
            };

            // 主题描述类
            struct Topic
            {
                std::mutex _mutex;
                std::string topic_name;                     // 主题名称
                std::unordered_set<Subscriber::ptr> subscribers; // 当前主题订阅者

                using ptr = std::shared_ptr<Topic>;

                Topic(const std::string &name) 
                    :topic_name(name)
                {}

                // 新增订阅者的时候使用
                void appendSubscriber(const Subscriber::ptr& subscriber)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    subscribers.insert(subscriber);
                }

                // 取消订阅 或 订阅连接者断开 的时候调用
                void removeSubscriber(const Subscriber::ptr& subscriber)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    subscribers.erase(subscriber);
                }

                // 收到消息发布请求的时候调用
                void pushMessage(const BaseMessage::ptr& msg)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    for(auto& subscriber : subscribers)
                    {
                        subscriber->conn->send(msg);
                    }
                }
            };

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, Topic::ptr> _topics;    // 主题名称 与 主题 的映射
            std::unordered_map<BaseConnection::ptr, Subscriber::ptr> _subscribers;    // 连接 与 订阅者 的映射

        public:
            using ptr = std::shared_ptr<TopicManager>;
            
            TopicManager()
            {}

            // 针对主题请求响应
            void onTopicRequest(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
            {
                LOG(DEBUG, "针对主题请求响应\n");
                TopicOptype topic_optype = msg->optype();
                bool ret = true;
                switch (topic_optype)
                {
                    case TopicOptype::TOPIC_CREATE:
                        topicCreate(conn, msg);
                        break;
                    case TopicOptype::TOPIC_REMOVE:
                        topicRemove(conn, msg);
                        break;
                    case TopicOptype::TOPIC_SUBSCRIBE:
                        topicSubscribe(conn, msg);
                        break;
                    case TopicOptype::TOPIC_CANCEL:
                        topicCancel(conn, msg);
                        break;
                    case TopicOptype::TOPIC_PUBLISH:
                        topicPublish(conn, msg);
                        break;
                    default:
                        return errorResponse(conn, msg, RCode::RCODE_INVALID_OPTYPE);
                }
                if(!ret)
                    return errorResponse(conn, msg, RCode::RCODE_NOT_FOUND_TOPIC);
                
                return topicResponse(conn, msg);
            }

            // 一个订阅者在连接断开时的处理---删除其关联的数据
            void onShutdown(const BaseConnection::ptr &conn)
            {
                // 消息发布者断开连接，不需要任何操作；  消息订阅者断开连接需要删除管理数据
                // 1. 判断断开连接的是否是订阅者，不是的话则直接返回
                std::vector<Topic::ptr> topics;
                Subscriber::ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto sub_it = _subscribers.find(conn);
                    if(sub_it == _subscribers.end())
                    {
                        // 断开的连接不是一个订阅者的连接
                        return;
                    }
                    subscriber = sub_it->second;
                    // 2. 获取到订阅者退出，受影响的主题对象
                    for(auto& topic_name : subscriber->topics)
                    {
                        auto topic_it = _topics.find(topic_name);
                        if(topic_it == _topics.end())
                            continue;
                        topics.push_back(topic_it->second);
                    }
                    // 4. 从订阅者映射信息中删除订阅者
                    _subscribers.erase(sub_it);
                }
                // 3. 从受影响的主题对象中，删除订阅者
                for(auto& topic : topics)
                {
                    topic->removeSubscriber(subscriber);
                }
            }

            private:
                // 发回错误响应
                void errorResponse(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg, RCode rcode)
                {
                    // 构建响应
                    auto msg_rsp = MessageFactory::create<TopicResponse>();
                    msg->SetMytype(MType::RSP_TOPIC);
                    msg_rsp->SetId(msg->rid());
                    msg_rsp->setRCode(rcode);
                    conn->send(msg_rsp);
                }

                // 发回正确响应
                void topicResponse(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
                {
                    // 构建响应
                    auto msg_rsp = MessageFactory::create<TopicResponse>();
                    msg_rsp->SetMytype(MType::RSP_TOPIC);
                    msg_rsp->SetId(msg->rid());
                    msg_rsp->setRCode(RCode::RCODE_OK);
                    conn->send(msg_rsp);
                }

                // 请求新建一个主题
                void topicCreate(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    // 构造一个主题对象，添加映射关系的管理
                    std::string topic_name = msg->topicKey();
                    auto topic = std::make_shared<Topic>(topic_name);
                    _topics.insert(std::make_pair(topic_name, topic));
                }

                // 删除一个主题
                void topicRemove(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
                {
                    // 1. 查看当前主题，有哪些订阅者，然后从订阅者中将主题信息删除掉
                    // 2. 删除主题的数据 -- 主题名称与主题对象的映射关系
                    std::string topic_name = msg->topicKey();
                    std::unordered_set<Subscriber::ptr> subscribers;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        // 在删除主题之前，先找出会受到影响的订阅者
                        auto it = _topics.find(topic_name);
                        if(it == _topics.end())
                        {
                            return;
                        }
                        subscribers = it->second->subscribers;
                        _topics.erase(topic_name);
                    }
                    // 对应的订阅者删除主题
                    for(auto& subscriber : subscribers)
                    {
                        subscriber->removeTopic(topic_name);
                    }
                }

                // 主题订阅
                bool topicSubscribe(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
                {
                    // 1. 先找出主题对象，以及订阅者对象
                    // 如果没有找到主题--就要报错；  但是如果没有找到订阅者对象，说明是第一次订阅，那就要构造一个订阅者
                    Topic::ptr topic;
                    Subscriber::ptr subscriber;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto topic_it = _topics.find(msg->topicKey());
                        if(topic_it == _topics.end())
                        {
                            return false;
                        }
                        topic = topic_it->second;
                        auto sub_it = _subscribers.find(conn);
                        if(sub_it != _subscribers.end())
                        {
                            subscriber = sub_it->second;
                        }
                        else
                        {
                            subscriber = std::make_shared<Subscriber>(conn);
                            _subscribers.insert(std::make_pair(conn, subscriber));
                        }
                    }
                    //2. 在主题对象中，新增一个订阅者对象关联的连接；  在订阅者对象中新增一个订阅的主题
                    topic->appendSubscriber(subscriber);
                    subscriber->appendTopic(topic->topic_name);
                    return true;
                }
                
                // 取消订阅
                void topicCancel(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
                {
                    // 1. 先找出主题对象，和订阅者对象
                    Topic::ptr topic;
                    Subscriber::ptr subscriber;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto topic_it = _topics.find(msg->topicKey());
                        if(topic_it != _topics.end())
                        {
                            topic = topic_it->second;
                        }
                        
                        auto sub_it = _subscribers.find(conn);
                        if(sub_it != _subscribers.end())
                        {
                            subscriber = sub_it->second;
                        }
                    }
                    // 2. 从主题对象中删除当前的订阅者连接； 从订阅者信息中删除所订阅的主题名称
                    if(subscriber) subscriber->removeTopic(msg->topicKey());
                    if(topic && subscriber) topic->removeSubscriber(subscriber);
                }

                bool topicPublish(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
                {
                    // 1. 先找出主题对象
                    Topic::ptr topic;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);
                        auto topic_it = _topics.find(msg->topicKey());
                        if(topic_it == _topics.end())
                        {
                            return false;
                        }
                        topic = topic_it->second;
                    }
                    topic->pushMessage(msg);
                    return true;
                }
        };

        
        
    };
};