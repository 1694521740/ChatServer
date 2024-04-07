#include "groupmodel.h"
#include "db.h"

//model层
// 创建群组
bool GroupModel::createGroup(Group &group)
{
    // 1.组装sql语句
    char sql[1024] = {0}; // id是自动生成的 在群组表中创建群，只需要表名称和表描述
    sprintf(sql, "insert into AllGroup(groupname, groupdesc) values('%s', '%s')",
            group.getName().c_str(), group.getDesc().c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的主键id并赋给该群
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}
// 加入群聊    用户名称、群名称、群角色
void GroupModel::addGroup(int userid, int groupid, string role)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into GroupUser values('%d', '%d', '%s')",
            groupid, userid, role.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    /*
    1.先根据userid在groupuser表中查询出该用户所属的群组消息
    2.在根据群组消息，查询属于该群组的所有用户的userid，并且和user表进行多表联合查询，查出用户的详细信息
    根据用户自身id查询groupuser得到所在的所有群聊消息，之后查询该群中所以成员消息，在从user表中查成员信息
    */
    char sql[1024] = {0}; // a.id是群id表a的，等于输入的群id 意思是把成员所在的群和群信息都找出来 为了不频繁使用数据库 inner join是连接表与表
    //根据给定的用户id来从Groupuser中找到对应的群，并根据群id相同这个条件来找到allgroup群的id、groupname、groupdesc
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from AllGroup a inner join \
    GroupUser b on a.id = b.groupid where b.userid=%d",
            userid);

    vector<Group> groupVec; 
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            // 查出userid所有的群组信息
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }
    // 查询群组的用户信息
    for (Group &group : groupVec)
    {
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from User a inner join \
    GroupUser b on b.userid = a.id where b.groupid=%d",
                group.getId()); //遍历所属群中用户的信息  

        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            // 查出userid所有的群组信息
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            mysql_free_result(res);
        }
    }
    return groupVec;
}

// 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其他用户群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    // 1.组装sql语句
    char sql[1024] = {0}; // 存储该群聊中除了我自己以外所有人的id进行保存发送信息
    sprintf(sql, "select userid from GroupUser where groupid = %d and userid != %d", groupid, userid);

    vector<int> idVec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                idVec.push_back(atoi(row[0]));
            }
            mysql_free_result(res);
        }
    }
    return idVec;
}