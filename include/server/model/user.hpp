#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

// 匹配User表的ORM(映射表)类
class User
{
public:
    // 初始化 - 给定默认初始值，不传实参时，自动使用默认初值
    User(int id = -1, string name = "", string pwd = "", string state = "offline")
    {
        this->id = id;
        this->name = name;
        this->password = pwd;
        this->state = state;
    }

    // 4个参数设置
    void setId(int id) { this->id = id; }
    void setName(string name) { this->name = name; }
    void setPwd(string pwd) { this->password = pwd; }
    void setState(string state) { this->state = state; }

    // 4个参数获取
    int getId() { return (this->id); }
    string getName() { return (this->name); }
    string getPwd() { return (this->password); }
    string getState() { return (this->state); }

private:
    int id;          // 用户id
    string name;     // 用户名
    string password; // 用户密码
    string state;    // 当前登录状态
};

#endif