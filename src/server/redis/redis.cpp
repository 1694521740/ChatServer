#include "redis.h"
#include <iostream>
using namespace std;

Redis::Redis() : _publish_context(nullptr), _subscribe_context(nullptr)
{
}

Redis::~Redis()
{
    if (_publish_context != nullptr)
    {
        redisFree(_publish_context);
    }
    if (_subscribe_context != nullptr)
    {
        redisFree(_subscribe_context);
    }
}

bool Redis::connect()
{
    // 负责publish发布消息的上下文连接
    _publish_context = redisConnect("127.0.0.1", 6379);
    if (_publish_context == nullptr)
    {
        cerr << "connect redis failed" << endl;
        return false;
    }
    // 负责subscribe发布消息的上下文连接
    _subscribe_context = redisConnect("127.0.0.1", 6379);
    if (_subscribe_context == nullptr)
    {
        cerr << "connect redis failed" << endl;
        return false;
    }
    // 在单独的线程中，监听通道上的事件，有消息给业务层进行上报
    // 只对一个线程堵塞，对消息进行监听
    thread t([&]()
             { observer_channel_message(); });
    t.detach();
    cout << "connect redis_server success!" << endl;
    return true;
}

// 向redis指定的通道channel发布消息 指令：PUBLISH 13（通道数） "hello" （消息）
// 这个指令一执行马上就回复了，不会阻塞，所以之间用redisCommand就好
bool Redis::publish(int channel, string message)
{
    redisReply *reply = (redisReply *)redisCommand(_publish_context, "PUBLISH %d %s", channel, message.c_str());
    if (reply == nullptr)
    {
        cerr << "publish commmand failed!" << endl;
        return false;
    }
    freeReplyObject(reply);
    return true;
}

// 向redis指定的通道subscribe订阅消息
// subscrib指令会阻塞线程所以我们不直接发送指令，而是先组装命令写到缓存中，在从缓存中发送到redisserver中，只订阅，不接受消息不阻塞
bool Redis::subscribe(int channel)
{
    // SUBSCRIBE命令本身会造成线程堵塞等待通道里面发生消息，这里只做订阅通道，不接受通道消息
    // 通道消息的接受专门在observer_channel_message函数中的独立线程只进行
    // 只负责发送命令，不阻塞接受redis server响应消息，否则和notifyMsg线程抢占资源
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "SUBSCRIBE %d", channel))
    {
        cerr << "subscribe command failed" << endl;
        return false;
    }
    // redisBufferWrite可以循环发送缓冲区，直到缓冲区数据发送完毕（done被置为1）
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "subscribe command failed!" << endl;
            return false;
        }
    }
    // redisCommand=redisAppendCommand->redisBufferWrite->redisGetReply
    // redisAppendCommand组装命令写到缓存中，redisBufferWrite从缓存中发送到redisserver中，
    // redisGetReply以阻塞的方式等待远端响应

    return true;
}

bool Redis::unsubscribe(int channel)
{
    if (REDIS_ERR == redisAppendCommand(this->_subscribe_context, "UNSUBSCRIBE %d", channel))
    {
        cerr << "unsubscribe command failed" << endl;
        return false;
    }
    int done = 0;
    while (!done)
    {
        if (REDIS_ERR == redisBufferWrite(this->_subscribe_context, &done))
        {
            cerr << "unsubscribe command failed!" << endl;
            return false;
        }
    }
    return true;
}

void Redis::observer_channel_message()
{
    redisReply *reply = nullptr;
    while (REDIS_OK == redisGetReply(this->_subscribe_context, (void **)&reply))
    {
        // 订阅收到的消息是一个带三元素的数组
        // subscribe监听的数据情况：（1）“message” (2)"13" (3)"hello world!";element[1]是通道，element[2]是消息。
        if (reply != nullptr && reply->element[2] != nullptr && reply->element[2]->str != nullptr)
        {
            // 给业务层上报通道上发生的消息 element[1]是通道，element[2]是消息。
            _notify_message_handler(atoi(reply->element[1]->str), reply->element[2]->str);
        }
        freeReplyObject(reply);
    }
    cerr << ">>>>>>>>>>>>>>>observer_channel_message quit <<<<<<<<<<<<<<<<" << endl;
}
void Redis::init_notify_handler(function<void(int, string)> fn)
{
    this->_notify_message_handler = fn;
}
