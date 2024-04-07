#include "chatserver.h"
#include "chatservice.h"
#include "json.h"
#include <functional>
#include <string>
using namespace std;
using namespace placeholders;
using json = nlohmann::json;

/*  聊天服务器网络部分
当创建一个 ChatServer对象时，构造函数 ChatServer::ChatServer 会被调用。
需要传入三个对象loop 服务器监听ip和端口 服务器名称 开始监听网络事件

随后将_server对象与成员函数（回调函数）绑定，将成员函数与对象实例绑定
loop对象的作用是启动事件循环并持续监听事件。一旦事件循环启动，
它就会一直运行在一个循环中，不断地检查是否有事件发生，并在事件
发生时调用注册的相应的回调函数来处理事件。
这样不同端口有不同的回调函数
*/
ChatServer::ChatServer(EventLoop *loop,               // 事件循环
                       const InetAddress &listenAddr, // 服务器监听的 Ip以及端口地址
                       const string &nameArg)         // 服务器名称
    : _server(loop, listenAddr, nameArg), _loop(loop) // ：意味着给构造函数进行初始化参数
{
    // 给服务器注册用户连接的创建和断开回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, _1)); // 将on..与当前对象this绑定，并将连接对象作为参数传递给on..
    // 将当前对象的on函数绑定到回调函数上，每次有新连接建立时调用on函数，并将连接对象作为参数传递给on函数

    // 给服务器注册用户读写事件回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    // 设置服务器端的线程数量
    _server.setThreadNum(4); // 一个作为io线程，剩下3个作为worker线程
}
// 开启事件循环
void ChatServer::start()
{
    _server.start();
}

// 专门处理用户的连接创建和断开 epoll listenfd accept 不需要考虑什么时候进行
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    // 客户端断开链接
    if (!conn->connected())
    {
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
    else
    {
    }
}
// 网络模块和业务模块通过事件回调完全区分开了
// 专门处理用户的读写事件  何时发生不用管
void ChatServer::onMessage(const TcpConnectionPtr &conn, // 连接
                           Buffer *buffer,               // 缓冲区 数据存放在缓存区内
                           Timestamp time)               // 接受数据的时间信息
{
    string buf = buffer->retrieveAllAsString(); // 获取缓冲区的数据并存放到字符串中
    // 数据的反序列化
    json js = json::parse(buf);
    // 需要达到的目的：完全解耦网络模块的代码和业务模块的代码
    // 根据js[“msgid”] 来获取=》 业务处理器handler =》conn js time
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    // 回调消息对应绑定好的事件处理器，来执行相应的业务处理
    msgHandler(conn, js, time);
}