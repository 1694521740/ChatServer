#include "friendmodel.h"
#include "db.h"


// 添加好友业务
void FriendModel::insert(int userid, int friendid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into Friend values(%d, '%d')", userid, friendid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
// 返回用户好友列表  但是friend表只有好友id，所以用这个好友id去user表中查询信息后在返回，查询姓名，状态
// 做一个两个表的联合查询
vector<User> FriendModel::query(int userid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    //这指令需要理解，输入一个userid，从表friend里面依次选取friend
    sprintf(sql, "select a.id,a.name,a.state from User a inner join Friend b on b.friendid = a.id where b .userid=%d", userid); 

    vector<User> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 查询成功
            MYSQL_ROW row;
            //把userid用户的所有离线消息放入vec中返回 月多行
            while((row = mysql_fetch_row(res))!= nullptr)
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;

        }
    }
    return vec;

}