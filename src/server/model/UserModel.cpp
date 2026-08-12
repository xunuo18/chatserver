#include "UserModel.hpp"
#include "db.h"
#include <string>
using namespace std;

// User表的增加方法
bool UserModel::insert(User &user)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into User(name,password,state) values('%s','%s','%s')",
            user.getName().c_str(), user.getPwd().c_str(), user.getState().c_str());
    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 根据用户id号查询用户信息
User UserModel::query(int id)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select * from User where id = %d", id);
    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        // 获取查找后的结果集句柄MYSQL_RES*
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 查找成功：读取数据(用户数据不允许重复，则查找结果肯定一行)
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr)
            {
                // row通过[]访问每个字段
                User user;
                user.setId(atoi(row[0])); // 把「数字形式的字符串」转换成「十进制int整型」
                user.setName(row[1]);
                user.setPwd(row[2]);
                user.setState(row[3]);
                // 访问完数据后，要把res释放
                mysql_free_result(res);
                return user;
            }
        }
    }
    return User(); // 没查到，返回空对象
}

// 更新用户的状态信息
bool UserModel::updateState(User &user)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "update User set state = '%s' where id = %d", user.getState().c_str(), user.getId());
    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            return true;
        }
    }
    return false;
}

// 重置用户的状态信息
void UserModel::resetState()
{
    // 1.组装sql语句
    char sql[1024] = "update User set state = 'offline' where state = 'online'";

    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
