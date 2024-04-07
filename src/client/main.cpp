#include "json.h"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "group.h"
#include "user.h"
#include "public.h"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
// 显示当前登录成功用户的基本信息
void showCurrentUserData();
// 控制menu界面的全局变量
bool isMainMenuRunning = false;

// 接受线程
void readTaskHandler(int clientfd);
// 获取系统事件（聊天消息需要添加时间信息）
string getCurrentTime();
// 主聊天页面程序 登录成功以后
void mainMenu(int clientfd);
// 聊天客户端程序实现，main线程用作发送线程，子线程用作接受线程
int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 127.0.0.1 6000" << endl;
        exit(-1);
    }
    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t pory = atoi(argv[2]);

    // 创建client端的socket
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }
    // 填写client需要连接的server信息ip+port
    sockaddr_in server;
    memset(&server, 0, sizeof(sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(pory);
    server.sin_addr.s_addr = inet_addr(ip);

    // client和server进行连接
    if (-1 == connect(clientfd, (sockaddr *)&server, sizeof(sockaddr_in)))
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }
    // 死循环 main线程用于接受用户输入，负责发送数据
    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "========================" << endl;
        cout << "1.login" << endl;
        cout << "2.register" << endl;
        cout << "3.quit" << endl;
        cout << "========================" << endl;
        cout << "choice:" << endl;
        int choice;
        cin >> choice;
        cin.get(); // 读取缓冲区残留的回车

        switch (choice)
        {
        case 1: // login业务
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> id;
            cin.get(); // 读取缓冲区残留的回车
            cout << "userpassword:";
            cin.getline(pwd, 50); // 用于从标准输入流中读取一行文本

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send login msg error" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv login response error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);
                    if (0 != responsejs["errno"].get<int>()) // 登录失败
                    {
                        cerr << responsejs["errmsg"] << endl;
                    }
                    else // 登录成功
                    {
                        // 记录当前用户的id和name
                        g_currentUser.setId(responsejs["id"].get<int>());
                        g_currentUser.setName(responsejs["name"]);

                        // 记录当前用户的好友列表信息
                        if (responsejs.contains("friends"))
                        {
                            // 初始化
                            g_currentUserFriendList.clear();
                            vector<string> vec = responsejs["friends"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                User user;
                                user.setId(js["id"].get<int>());
                                user.setName(js["name"]);
                                user.setState(js["state"]);
                                g_currentUserFriendList.push_back(user);
                            }
                        }
                        // 记录当前用户的群组列表信息
                        if (responsejs.contains("groups"))
                        {
                            g_currentUserGroupList.clear(); // 初始化
                            vector<string> vec1 = responsejs["groups"];
                            for (string &groupstr : vec1)
                            {
                                json grpjs = json::parse(groupstr);
                                Group group;
                                group.setId(grpjs["id"].get<int>());
                                group.setName(grpjs["groupname"]);
                                group.setDesc(grpjs["groupdesc"]);

                                vector<string> vec2 = grpjs["users"];
                                for (string &userstr : vec2)
                                {
                                    GroupUser user;
                                    json js = json::parse(userstr);
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    user.setRole(js["role"]);
                                    group.getUsers().push_back(user);
                                }
                                g_currentUserGroupList.push_back(group);
                            }
                        }
                        // 显示登录用户的基本信息
                        showCurrentUserData();
                        // 显示当前用户的离线消息 个人聊天信息或者群组消息
                        if (responsejs.contains("offlinemsg"))
                        {
                            vector<string> vec = responsejs["offlinemsg"];
                            for (string &str : vec)
                            {
                                json js = json::parse(str);
                                // time + [id] + name + "said:" + xxx
                                int msgtype = js["msgid"].get<int>();
                                if (ONE_CHAT_MSG == msgtype)
                                {
                                    cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                                         << "said:" << js["msg"].get<string>() << endl;
                                    continue;
                                }
                                else
                                {
                                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                                         << "said:" << js["msg"].get<string>() << endl;
                                }
                            }
                        }
                        // 登录成功 启动接受线程负责接受数据 子线程 该线程只启动一次
                        //重复登录的话 不能启动多个，只启动一个就行了
                        static int readthreadnumber = 0;
                        if (readthreadnumber == 0)
                        {
                            std::thread readTask(readTaskHandler, clientfd); // 底层就是相当于调用pthread_create
                            readTask.detach();                               // pthread_detach 设置成一个分离线程，线程运行完自动回收线程所占用的内核pcb资源
                            readthreadnumber++;
                        }

                        // 进入聊天主菜单页面
                        isMainMenuRunning = true;
                        mainMenu(clientfd);
                    }
                }
            }
        }
        break;
        case 2: // register业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50); // 默认遇到回车才结束
            cout << "userpassword:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();

            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
            {
                cerr << "send reg msg error:" << request << endl;
            }
            else
            {
                char buffer[1024] = {0};
                len = recv(clientfd, buffer, 1024, 0);
                if (-1 == len)
                {
                    cerr << "recv reg response error" << endl;
                }
                else
                {
                    json responsejs = json::parse(buffer);   // 反序列化
                    if (0 != responsejs["errno"].get<int>()) // 注册失败
                    {
                        cerr << name << " is already exist, register error!" << endl;
                    }
                    else // 注册成功
                    {
                        cout << name << " register success, userid is " << responsejs["id"]
                             << ", do not forget it!" << endl;
                    }
                }
            }
        }
        break;
        case 3: // quit业务
            close(clientfd);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }
}
// 接受线程
void readTaskHandler(int clientfd)
{
    for (;;) //  死循环
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0); // 接受数据 阻塞了 如果当前用户登出该线程也无法响应 只启动一次就行
        if (-1 == len || 0 == len)
        {
            close(clientfd);
            exit(-1);
        }
        // 接受ChatServer转发的数据，反序列化生成json数据对象，在通过[]运算符重载函数，获取对应信息
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (ONE_CHAT_MSG == msgtype)
        {
            cout << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << "said:" << js["msg"].get<string>() << endl;
            continue;
        }
        if (GROUP_CHAT_MSG == msgtype)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << "[" << js["id"] << "]" << js["name"].get<string>()
                 << "said:" << js["msg"].get<string>() << endl;
            continue;
        }
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "==================login user==================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "------------------friend list------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "------------------group list------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
    }
    cout << "==============================================" << endl;
}

// 方法的声明
//  ”help“ command handler
void help(int fd = 0, string str = "");
//"chat" command handler
void chat(int, string);
//"addfriend" command handler
void addfriend(int, string);
//"creategroup" command handler
void creategroup(int, string);
//"addgroup" command handler
void addgroup(int, string);
//"groupchat" command handler
void groupchat(int, string);
//"loginout" command handler
void loginout(int, string);

// 系统支持的客户端命令列表
// 表驱动的设计
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群聊,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,格式addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"loginout", "注销,格式loginout"},
};
// 注册系统支持的客户端命令处理 键是string指令，值是函数对象类型。设置map将命令和函数对应起来
// 如果有对应的命令就执行对应函数，如果没有指令就报错。
// 需要增加指令和命令就比较简单
// int是socketfd string是需要发送的数据
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}};

// 主聊天页面程序
void mainMenu(int clientfd)
{
    // 一进来先给大家显示一下相应的命令
    help(); // 有默认值
    char buffer[1024] = {0};
    // 使用全局变量，来循环界面，不用死循环，因为死循环之后loginout无法退出
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);      // 输入buffer
        string commandbuf(buffer);      // 转buffer为commandbuf
        string command;                 // 存储命令
        int idx = commandbuf.find(":"); // 寻找是否存在：如果没有则整个当作命令 loginout和help是没有：的
        if (-1 == idx)
        {
            command = commandbuf;
        }
        else // 存在冒号提取冒号之前的部分赋给command
        {
            command = commandbuf.substr(0, idx); // idx是冒号的位置从0-：位置
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }
        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法
        // commandbuf.substr(idx + 1, commandbuf.size()将第一个冒号后面的内容赋给函数
    }
}
//"help"command handler
void help(int, string)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap) // 打印指令表
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}
// "addfriend" command handler
// clientfd是客户端的文件描述符,通过这个文件描述符，服务器可以向特定的客户端发送数据或接收来自客户端的数据。
// 在网络编程中，通常使用socket函数创建一个套接字，
// 并使用 accept 函数接受客户端连接，accept 函数返回的文件描述符就是 clientfd
void addfriend(int clientfd, string str)
{
    int friendid = atoi(str.c_str()); // 转为整数
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump(); // 进行json序列化，进行发送

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0); // strlen数据长度+1，包含空字符 发送
    if (-1 == len)                                                           // 发送失败
    {
        cerr << "send addfriend msg error ->" << buffer << endl;
    }
}
//"chat" command handler
void chat(int clientfd, string str)
{
    int idx = str.find(":"); // chat指令是friendid:message 第一个：前是对应函数，传入的内容是第一个：后的是friendid:message
    if (-1 == idx)
    {
        cerr << "chat command invalid!" << endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());        // 将friendid:message查找出：，：前的是friendid
    string message = str.substr(idx + 1, str.size() - idx); // :后的是消息

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId(); // 当前用户的id
    js["name"] = g_currentUser.getName();
    js["to"] = friendid; // 对应的消息一定要弄清楚
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump(); // 序列化
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error ->" << buffer << endl;
    }
}
//"creategroup" command handler
void creategroup(int clientfd, string str)
{
    int idx = str.find(":"); // groupname:groupdesc
    if (-1 == idx)
    {
        cerr << "creategroup command invalid!" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    js["grouprole"] = "creator";
    string buffer = js.dump(); // 序列化
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send chat msg error ->" << buffer << endl;
    }
}
//"addgroup" command handler
void addgroup(int clientfd, string str) // addgroup:groupid
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    // js["grouprole"] = "normal";
    string buffer = js.dump(); // 序列化
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send addgroup msg error ->" << buffer << endl;
    }
}

//"groupchat" command handler
void groupchat(int clientfd, string str)
{
    int idx = str.find(":"); // groupid:message
    if (-1 == idx)
    {
        cerr << "groupchat command invalid!" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());         // 将friendid:message查找出：，：前的是friendid
    string message = str.substr(idx + 1, str.size() - idx); // :后的是消息
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId(); // 当前用户的id
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump(); // 序列化
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (-1 == len)
    {
        cerr << "send groupchat msg error ->" << buffer << endl;
    }
}
//"loginout" command handler
void loginout(int clientfd, string str)
{
    // 退出菜单
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump(); // 进行json序列化，进行发送

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0); // strlen数据长度+1，包含空字符 发送
    if (-1 == len)                                                           // 发送失败
    {
        cerr << "send loginout msg error ->" << buffer << endl;
    }
    else
    {
        // 推出menu
        isMainMenuRunning = false;
    }
}

string getCurrentTime()
{
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}