#ifndef FRIENDMODEL_H
#define FRIENDMODEL_H
#include "user.h"
#include <vector>
using namespace std;

// 维护好友信息的操作接口方法
class FriendModel
{
public:
    // 添加好友业务
    void insert(int userid, int friendid);
    // 返回用户好友列表  但是friend表只有好友id，所以用这个好友id去user表中查询信息后在返回，查询姓名，状态
    // 做一个两个表的联合查询
    vector<User> query(int userid);
};

#endif