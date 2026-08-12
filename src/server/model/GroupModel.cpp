#include "GroupModel.hpp"
#include "db.h"
#include <cstring>

// 创建群组
bool GroupModel::createGroup(Group &group)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into AllGroup(groupname,groupdesc) values('%s', '%s')",
            group.getName().c_str(), group.getDesc().c_str());
    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取创建群组成功的群组的主键id
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}
// 加入群组
void GroupModel::addGroup(int userid, int groupid, string role)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into GroupUser(groupid,userid,grouprole) values(%d, %d,'%s')",
            groupid, userid, role.c_str());
    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    // 1.组装sql语句
    /*
    1.先根据userid在GroupUser表中查询出该用户所属的群组信息
    2.再根据群组信息，查询属于该群组的所有用户的userid，并且和User表进行多表联合查询，查出用户的详细信息
    */
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from AllGroup a inner join \
    GroupUser b on a.id = b.groupid where b.userid=%d",
            userid);

    vector<Group> groupVec;

    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        // 1.查询当前用户所属的群组
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 查找成功：读取userid的所有群组信息
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                Group group;
                group.setId(atoi(row[0]));
                group.setName(row[1]);
                group.setDesc(row[2]);
                groupVec.push_back(group);
            }
            // 访问完数据后，要把res释放
            mysql_free_result(res);
        }

        // 2.查询每个群组中的全部成员
        for (Group &group : groupVec)
        {
            memset(sql, 0, sizeof(sql));

            snprintf(sql,
                     sizeof(sql),
                     "select a.id, a.name, a.state, b.grouprole from User a inner join \
                      GroupUser b on a.id = b.userid where b.groupid = %d",
                     group.getId());

            MYSQL_RES *userRes = mysql.query(sql);

            if (userRes != nullptr)
            {
                MYSQL_ROW userRow;
                while ((userRow = mysql_fetch_row(userRes)) != nullptr)
                {
                    GroupUser user;
                    user.setId(atoi(userRow[0]));
                    user.setName(userRow[1]);
                    user.setState(userRow[2]);
                    user.setRole(userRow[3]);

                    group.getUsers().push_back(user);
                }
                mysql_free_result(userRes);
            }
        }
    }
    return groupVec; // 没查到，返回空对象
}

// 根据指定的groupid查询群组用户id列表，除userid自己，主要用户群聊业务给群组其它成员群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select userid from GroupUser where groupid=%d and userid!=%d", groupid, userid);

    vector<int> idVec;

    // 2.定义MySQL对象
    MySQL mysql;
    if (mysql.connect())
    {
        // 获取查找后的结果集句柄MYSQL_RES*
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            // 查找成功：读取userid的所有群组信息

            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                idVec.push_back(atoi(row[0]));
            }

            // 访问完数据后，要把res释放
            mysql_free_result(res);
        }
    }
    return idVec; // 没查到，返回空对象
}
