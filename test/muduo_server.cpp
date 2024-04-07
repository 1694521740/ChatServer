/*
muduo������û��ṩ��������Ҫ����
Tcpserver �� ���ڱ�д�����������
Tcpclient �� ���ڱ�д�ͻ��˳����

���Ƿ�װ��epoll+�̳߳�  
�ô����ܹ�������I/O�Ĵ����ҵ��������ֿ�
			ֻ��Ҫ��ע��Щ	�û������ӺͶϿ� �û��Ŀɶ�д�¼�
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
1.���Tcpserver����
2.����EvenLoop�¼�ѭ�������ָ��
3.��ȷTcpServer���캯����Ҫʲô����
4.�ڵ�ǰ��������Ĺ��캯�����У�ע�ᴦ�����ӵĻص������ʹ����д�¼��Ļص�����
5.���ú��ʵķ�����߳����� muduo����Լ�����io�̺߳�worker�߳�
*/

class CharServer
{
public:

	CharServer(EventLoop* loop,               //�¼�ѭ��
		const InetAddress& listenAddr,    //������������ Ip�Լ��˿ڵ�ַ
		const string& nameArg)				// ����������
		:_server(loop, listenAddr, nameArg), _loop(loop)   //����ζ�Ÿ����캯�����г�ʼ������
	{
		// ��������ע���û����ӵĴ����ͶϿ��ص�
		_server.setConnectionCallback(std::bind(&CharServer::onConnection, this, _1));//��on..�뵱ǰ����this�󶨣��������Ӷ�����Ϊ�������ݸ�on..
		//����ǰ�����on�����󶨵��ص������ϣ�ÿ���������ӽ���ʱ����on�������������Ӷ�����Ϊ�������ݸ�on����



		// ��������ע���û���д�¼��ص�
		_server.setMessageCallback(std::bind(&CharServer::onMessage, this, _1, _2, _3));

		//���÷������˵��߳�����
		_server.setThreadNum(4);//һ����Ϊio�̣߳�ʣ��3����Ϊworker�߳�
	}
	//�����¼�ѭ��
	void start() 
	{
		_server.start();
	}
	
private:

	//ר�Ŵ����û������Ӵ����ͶϿ� epoll listenfd accept ����Ҫ����ʲôʱ�����
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
	//ר�Ŵ����û��Ķ�д�¼�
	void onMessage(const TcpConnectionPtr &conn,//����
				Buffer *buffer,      //������
				Timestamp time)//�������ݵ�ʱ����Ϣ
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
	loop.loop();//epoll_wait �������ķ�ʽ�ȴ����û����ӣ��������û��Ķ�д�¼���

	return 0;

}