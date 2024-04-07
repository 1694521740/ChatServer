/*
muduo网络给用户提供了两个主要的类
Tcpserver ： 用于编写服务器程序的
Tcpclient ： 用于编写客户端程序的

就是封装了epoll+线程池  
好处：能够把网络I/O的代码和业务代码区分开
			只需要关注这些	用户的连接和断开 用户的可读写事件
*/
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<iostream>
#include<functional>
using namespace std;
using namespace muduo;
using namespace muduo::net;
using namespace placeholders;
/*
1.组合Tcpserver对象
2.创建EvenLoop事件循环对象的指针
3.明确TcpServer构造函数需要什么参数
4.在当前服务器类的构造函数当中，注册处理连接的回调函数和处理读写事件的回调函数
5.设置合适的服务端线程数量 muduo库会自己分配io线程和worker线程
*/

class CharServer
{
public:

	CharServer(EventLoop* loop,               //事件循环
		const InetAddress& listenAddr,    //服务器监听的 Ip以及端口地址
		const string& nameArg)				// 服务器名称
		:_server(loop, listenAddr, nameArg), _loop(loop)   //：意味着给构造函数进行初始化参数
	{
		// 给服务器注册用户连接的创建和断开回调
		_server.setConnectionCallback(std::bind(&CharServer::onConnection, this, _1));//将on..与当前对象this绑定，并将连接对象作为参数传递给on..
		//将当前对象的on函数绑定到回调函数上，每次有新连接建立时调用on函数，并将连接对象作为参数传递给on函数



		// 给服务器注册用户读写事件回调
		_server.setMessageCallback(std::bind(&CharServer::onMessage, this, _1, _2, _3));

		//设置服务器端的线程数量
		_server.setThreadNum(4);//一个作为io线程，剩下3个作为worker线程
	}
	//开启事件循环
	void start() 
	{
		_server.start();
	}
	
private:

	//专门处理用户的连接创建和断开 epoll listenfd accept 不需要考虑什么时候进行
	void onConnection(const TcpConnectionPtr &conn)
	{
		if (conn->connected())
		{
			cout << conn->peerAddress().toIpPort() << "->" << conn->localAddress().toIpPort() << "state:online" << endl;
		}
		else
		{
			cout << conn->peerAddress().toIpPort() << "->" << conn->localAddress().toIpPort() << "state:outline" << endl;
			conn->shutdown();// close(fd)
			//_loop->quit();
		}

	}
	//专门处理用户的读写事件
	void onMessage(const TcpConnectionPtr &conn,//连接
				Buffer *buffer,      //缓冲区
				Timestamp time)//接受数据的时间信息
	{
		string buf = buffer->retrieveAllAsString();
		cout << "recv data:" << buf << "time:" << time.toString() << endl;
		conn->send(buf);
		
	}

	TcpServer _server;
	EventLoop *_loop;


};

int main()
{
	EventLoop loop;
	InetAddress addr("127.0.0.1",6000);
	CharServer server(&loop, addr, "charServer");

	server.start();
	loop.loop();//epoll_wait 以阻塞的方式等待新用户连接，已连接用户的读写事件等

	return 0;

}