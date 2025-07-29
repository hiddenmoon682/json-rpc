// #include "detail.hpp"
// #include "message.hpp"

// using namespace util_ns;
// using namespace std;

// int main()
// {
//     // std::shared_ptr<util_ns::RpcRequest> rrp = util_ns::MessageFactory::create<util_ns::RpcRequest>();

//     // rrp->setMethod("news");
//     // Json::Value val;
//     // val["num1"] = 11;
//     // val["num2"] = 22;
//     // rrp->setParms(val);

//     // cout << rrp->serialize() << endl;
//     // cout << rrp->check() << endl;

//     // std::shared_ptr<util_ns::TopicRequest> trp = util_ns::MessageFactory::create<util_ns::TopicRequest>();

//     // trp->setTopicKey("news");
//     // trp->setOptype(TopicOptype::TOPIC_PUBLISH);
//     // trp->setTopicMsg("this is a news");
//     // cout << trp->serialize() << endl;

//     // cout << trp->check() << endl;

//     // std::shared_ptr<util_ns::ServiceRequest> srp = util_ns::MessageFactory::create<util_ns::ServiceRequest>();

//     // srp->setMethod("Add");
//     // // srp->setOptype(ServiceOptype::SERVICE_DISCOVERY);
//     // srp->setOptype(ServiceOptype::SERVICE_ONLINE);
//     // Address host = make_pair("127.0.0.1", 8080);
//     // srp->setHost(host);

//     // cout << srp->serialize() << endl;
//     // cout << srp->check() << endl;

//     // std::shared_ptr<util_ns::RpcResponse> rrs= util_ns::MessageFactory::create<util_ns::RpcResponse>();

//     // rrs->setRCode(RCode::RCODE_OK);
//     // Json::Value val;
//     // val["result"] = "11 + 22 = 33";
//     // rrs->setResult(val);

//     // cout << rrs->serialize() << endl;
//     // cout << rrs->check() << endl;

//     std::shared_ptr<util_ns::ServiceResponse> rrs= util_ns::MessageFactory::create<util_ns::ServiceResponse>();
//     rrs->setMethod("Add");
//     rrs->setRCode(RCode::RCODE_OK);
//     rrs->setOptype(ServiceOptype::SERVICE_DISCOVERY);
//     std::vector<Address> addrs;
//     addrs.push_back(make_pair("127.0.0.1", 8080));
//     addrs.push_back(make_pair("127.0.0.1", 8888));

//     rrs->setHost(addrs);

//     cout << rrs->check() << endl;
//     cout << rrs->serialize() << endl;

//     return 0;
// }

