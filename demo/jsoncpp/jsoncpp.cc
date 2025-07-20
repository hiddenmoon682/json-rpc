#include <iostream>
#include <string>
#include <sstream>
#include <memory>

#include <jsoncpp/json/json.h>

bool Serialize(const Json::Value &root, std::string &str)
{
    Json::StreamWriterBuilder swb;
    std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());

    std::stringstream ss;
    int ret = sw->write(root, &ss);
    if(ret != 0)
    {
        std::cout << "Serialize failed!" << std::endl;
        return false;
    }
    str = ss.str();
    return true;
}

bool UnSerialize(const std::string &str, Json::Value &root)
{
    Json::CharReaderBuilder crb;
    std::unique_ptr<Json::CharReader> cr(crb.newCharReader());

    bool ret = cr->parse(str.c_str(), str.c_str() + str.size(), &root, nullptr);
    if(!ret)
    {
        std::cout << "UnSerialize failed!" << std::endl;
        return false;
    }
    return true;
}

int main()
{
    Json::Value v;
    v["name"] = "chen";
    std::string str;
    Serialize(v, str);
    std::cout << str << std::endl;

    Json::Value nv;
    UnSerialize(str, nv);
    std::cout << nv["name"].asString() << std::endl;

    return 0;
}
