#ifndef GROUPUSER_H
#define GROUPUSER_H
#include "user.h"

//添加了一个派生变量role 表示群组中哦用户的决策信息
class GroupUser : public User//从user中继承，包含其所有属性
{
public:
    void setRole(string role) { this->role = role; }
    string getRole() { return this->role; }

public:
    string role; 
};

#endif