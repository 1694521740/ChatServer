# ChatServer
基于muduo网络库实现的ChatServer服务器以及客户端的源码。
配置了nginx基于tcp的负载均衡，实现聊天服务器的集群功能，提高后端服务的并发能力。
使用mysql关系型数据库作为项目数据的落地存储。
基于redis的发布-订阅功能实现跨服务器的消息通信。
使用json序列化和反序列化作为私有化通信协议。

编译方式：
cd build
rm -rf *
cmake ..
make

需要nginx的tcp负载均衡
