#include "OfflineMessageModel.hpp"
#include <db.h>

// 存储用户的离线消息
void OfflineMsgModel::insert(int userid, string msg)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into OfflineMessage values(%d, '%s')", userid, msg.c_str());
    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
// 删除用户的离线消息
void OfflineMsgModel::remove(int userid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from OfflineMessage where userid=%d", userid);
    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
// 查询用户的离线消息(多个)
vector<string> OfflineMsgModel::query(int userid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select message from OfflineMessage where userid = %d", userid);
    // 2.定义MySQL对象
    MySQL mysql;
    vector<string> vec;
    if (mysql.connect())
    {
        // 获取查找后的结果集句柄MYSQL_RES*
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 查找成功：读取数据(消息有多行)，把userid用户的所有离线消息放到vec中返回
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
        }
    }
    return vec; // 没查到，返回空
}
