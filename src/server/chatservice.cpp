#include "chatservice.h"
#include "public.h"
#include <string>
#include <muduo/base/Logging.h>
#include "friendmodel.h"
#include <vector>
using namespace muduo;
using namespace std;
// 获取单例对象的接口函数  调用时创建一个单例对象
// 业务部分代码
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}
// Handler回调操作，设置一个map来存操作消息类型和处理操作，是绑定的
ChatService::ChatService()
{
    // 将消息类型与对应的操作进行绑定 使得每个消息类型对应一个处理器
    // login是对应操作，LOGIN_MSG只是public定义的一个代表这个操作的enum
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 设置redis服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调
        // redis底层会监听消息，并上报，这里需要预先做一个回调操作，以便上报：通道号、消息
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}
void ChatService::reset()
{
    // 把online用户的状态设置为offline
    _userModel.resetState();
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end()) // 如果没找到对应的操作
    {
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << "Can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid]; // 进行对于消息类型的操作，调用相应的函数
    }
}
// 处理登录业务
// 网络模块和业务模块通过事件回调完全区分开了
// 要将业务层和数据层也区分开 便于之后更改数据层
// ORM 业务层操作的都是对象 数据层
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            // 响应消息的错误标识
            response["errno"] = 2;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else
        {

            // 一个用户一个connection，登录成功就可以保存connection
            //  登录成功 记录用户的连接信息 需要考虑线程安全问题 用户上下线的改变会改变这个map
            // 只对连接这块加互斥锁
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn}); // 绑定id和连接并且存在map中，为了线程安全，所以每次用户三下线，需要改变这个map的时候都会使用互斥锁
            }

            // 登录成功之后，redis订阅channel=用户id
            _redis.subscribe(id);

            // 登录成功 更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            // 响应消息的错误标识
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            // 查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }

            // 查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            // 查询该用户的群组信息并返回
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grojson;
                    grojson["id"] = group.getId();
                    grojson["groupname"] = group.getName();
                    grojson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grojson["users"] = userV;
                    groupV.push_back(grojson.dump());
                }
                response["groups"] = groupV; // json序列化传输，客户端通过jsg[roups]进行取数据
            }

            conn->send(response.dump());
        }
    }
    else
    {
        // 用户存在但密码错误，登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        // 响应消息的错误标识
        response["errno"] = 1;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }
}
// 处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 获取用户名称与密码
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    // 将用户插入数据表中
    bool state = _userModel.insert(user);
    if (state)
    {
        // 注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        // 响应消息的错误标识
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        // 响应消息的错误标识
        response["errno"] = 1;
        conn->send(response.dump());
    }
}

// 处理客户端异常推出clientCloseException(conn);
// 1、从连接的map表中查询连接TcpConnectionPtr，查到后删除表中的这个信息
// 2、随后将这个用户的状态设置为off
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }
    // 用户下线了，就要在redis中取消订阅
    _redis.unsubscribe(user.getId());
    if (user.getId() != -1)
    {
        // 更新用户的状态信息
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }
    // 用户注销，相当于下线了，就要在redis中取消订阅
    _redis.unsubscribe(userid);

    User user(userid, "", "", "offline");
    // 更新用户的状态信息
    _userModel.updateState(user);
}

// 一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{

    int toid = js["to"].get<int>();
    {
        // 需要加锁的只有map表在线内容，锁的力度要尽量小 离开{}或者return 离开作用域都会释放这个锁
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            // toid在线，转发消息 服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }
    // 查询是否在线，如果在线，则前面的map中没有，但是在线，代表在别的服务器中
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }
    // toid不在线，存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}

// 添加好友业务 msgid 自身id 朋友friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);
}
// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>(); // 那个用户要创建群，群id是自动生成的
    string name = js["groupname"];
    string desc = js["groupdesc"];
    // 存储新创建的群组信息
    Group group(-1, name, desc);        // 初始id=-1
    if (_groupModel.createGroup(group)) // 调用mod_userConnMap层的创建群组，获取群消息 bool对象 群信息
    {
        // 存储群组创建人信息  调用加入群聊函数 这个是群内人员的信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}
// 加入群聊业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}
// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    // 获取群成员
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    // 遍历群成员进行循环发送

    // C++STL中哦Map不是线程安全的map
    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息 key是用户的id，值是用户的conn
            it->second->send(js.dump());
        }
        else
        {
            // 查询是否在线，如果在线，则前面的map中没有，但是在线，代表在别的服务器中
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
                return;
            }
            else
            {
                // 存储离线群消息 查找时用户不在线，则存储离线消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 从redis消息队列中获取订阅的消息
// 由redis调用的，来上报发送消息的通道和消息，用户在哪台服务器登录，哪台服务器就能收到
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    // 这一个离线消息是考虑到，可能用户1在给用户2发送消息时，在redis查询过错中用户下线了，
    // 所以在这台服务器查询一下，如果下线就离线消息
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }
    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}
