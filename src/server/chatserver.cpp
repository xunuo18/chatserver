#include "server/chatserver.hpp"
#include "server/chatservice.hpp"
#include "json.hpp"
#include <functional>
#include <iostream>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 注册链接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, placeholders::_1));
    // 注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, placeholders::_1, placeholders::_2, placeholders::_3));
    // 设置线程数量
    _server.setThreadNum(4);
    
}

void ChatServer::start()
{
    _server.start();
}

void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客服端断开链接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}

void ChatServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buffer,
                           Timestamp time)
{
    string buff = buffer->retrieveAllAsString();
    // 数据的反序列化
    json js = json::parse(buff);
    // 目的：完全解耦网络模块的代码和业务模块的代码
    // 方法1：使用面向接口的编程(C++中没有接口，即面向基类)
    // 方法2：回调函数
    // 1.通过js["msgid"]获取 → 业务处理器handler → conn js time
    // 2.get<T>()模板函数，将内容转换为其他类型，这里转换为int类型
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息绑定好的事件处理器,来执行相应的业务处理
    msgHandler(conn, js, time);
}
