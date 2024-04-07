#include "chatserver.h"
#include <iostream>
#include <signal.h>
#include <chatservice.h>
using namespace std;

//处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "Usage: " << argv[0] << " <address> <port>" << endl;
        return -1;
    }

    signal(SIGINT, resetHandler);
    EventLoop loop;
    InetAddress addr(argv[1], atoi(argv[2]));
    ChatServer server(&loop, addr, "chatServer");

    server.start();
    loop.loop(); // epoll_wait 以阻塞的方式等待新用户连接，已连接用户的读写事件等

    return 0;
}
