#ifndef GROUP_H
#define GROUP_H

#include "GroupUser.hpp"
#include <string>
#include <vector>
using namespace std;

// 匹配User表的ORM(映射表)类
class Group
{
public:
    // 初始化
    Group(int id = -1, string name = "", string desc = "")
    {
        this->id = id;
        this->groupname = name;
        this->groupdesc = desc;
    }

    // 3个参数设置
    void setId(int id) { this->id = id; }
    void setName(string name) { this->groupname = name; }
    void setDesc(string desc) { this->groupdesc = desc; }

    // 3个参数获取
    int getId() { return (this->id); }
    string getName() { return (this->groupname); }
    string getDesc() { return (this->groupdesc); }

    //
    vector<GroupUser> &getUsers(){return this->users;}

private:
    int id;
    string groupname;
    string groupdesc;
    vector<GroupUser> users;
};

#endif