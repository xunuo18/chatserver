#include "chatservice.hpp"
#include "public.hpp"
#include "Group.hpp"
#include <functional>
#include <string>
#include <string.h>
#include <muduo/base/Logging.h> //包含打印日志
#include <vector>
#include <map>
using namespace std;
using namespace muduo;

// 获取单例对象的接口函数
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    // 记录错误日志，msgid没有对应的事件处理回调
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        // 返回一个默认的处理器，空操作 - return [=](形参列表){ 函数体 };
        // [=]：以值捕获（拷贝）当前作用域里所有被 Lambda 内部用到的局部变量
        return [=](const TcpConnectionPtr &conn, json &js, Timestamp time)
        {
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
            // string errstr = "msgid:" + to_string(msgid) + " can not find handler!";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}

// 处理登录业务 id pwd(检测是否正确)
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 1.从js中获取id和pwd
    int id = js["id"].get<int>();
    string pwd = js["password"];

    // 2.根据id查找对应的User对象
    User user = _userModel.query(id);

    // 3.对比User对象的pwd是否与sql中的一致 - 用户存在且密码正确
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            // 该用户已经登录，不允许重复登录
            json reponse;
            reponse["msgid"] = LOGIN_MSG_ACK;
            reponse["errno"] = 2;
            reponse["errmsg"] = "this account is using, input another!";
            conn->send(reponse.dump());
        }
        else
        {
            // pwd一致，登录成功
            // ①记录用户连接信息 - 定义lock类，自动加锁解锁释放锁
            // 作用范围：所在{}
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            // ②更新用户状态信息:state offline → online
            user.setState("online");
            _userModel.updateState(user);
            // ③返回js消息
            json reponse;
            reponse["msgid"] = LOGIN_MSG_ACK;
            reponse["errno"] = 0;
            reponse["id"] = user.getId();
            reponse["name"] = user.getName();
            // ④查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(user.getId());
            if (!vec.empty())
            {
                // 读取该用户的离线消息，并组合到js发出
                reponse["offlinemsg"] = vec;
                // 读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(user.getId());
            }
            // ⑤查询该用户的好友信息
            vector<User> Friendrvec = _friendModel.query(user.getId());
            if (!Friendrvec.empty())
            {
                // 读取该用户的好友列表信息，并组合到js发出
                vector<string> vec_f;
                for (User user : Friendrvec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec_f.push_back(js.dump());
                }
                reponse["friends"] = vec_f;
            }
            // ⑥查询该用户的群组信息
            vector<Group> Grouprvec = _groupModel.queryGroups(user.getId());
            if (!Grouprvec.empty())
            {
                vector<string> vec_g;
                for (Group &group : Grouprvec)
                {
                    json groupjs;
                    groupjs["id"] = group.getId();
                    groupjs["groupname"] = group.getName();
                    groupjs["groupdesc"] = group.getDesc();

                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json userjs;
                        userjs["id"] = user.getId();
                        userjs["name"] = user.getName();
                        userjs["state"] = user.getState();
                        userjs["role"] = user.getRole();
                        userV.push_back(userjs.dump());
                    }
                    groupjs["users"] = userV;
                    vec_g.push_back(groupjs.dump());
                }
                reponse["groups"] = vec_g;
            }
            conn->send(reponse.dump());
        }
    }
    else
    {
        if (user.getId() != id)
        {
            // 用户不存在，登录失败
            json reponse;
            reponse["msgid"] = LOGIN_MSG_ACK;
            reponse["errno"] = 1;
            reponse["errmsg"] = "user not exist!";
            conn->send(reponse.dump());
        }
        else
        {
            // 用户存在但是pwd错误，登录失败
            json reponse;
            reponse["msgid"] = LOGIN_MSG_ACK;
            reponse["errno"] = 1;
            reponse["errmsg"] = "name or password is invaild!";
            conn->send(reponse.dump());
        }
    }
}

// 处理注册业务 name password
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 1.获取js里传进来的name和password字段内容
    string name = js["name"];
    string pwd = js["password"];
    // 2.创建User对象
    User user;
    user.setName(name);
    user.setPwd(pwd);
    // 3.插入新用户
    bool state = _userModel.insert(user);
    // 4.插入成功与否判断
    if (state)
    {
        // 注册成功
        json reponse;
        reponse["msgid"] = REG_MSG_ACK;
        reponse["errno"] = 0;
        reponse["id"] = user.getId();
        conn->send(reponse.dump());
    }
    else
    {
        // 注册失败
        json reponse;
        reponse["msgid"] = REG_MSG_ACK;
        reponse["errno"] = 1;
        conn->send(reponse.dump());
    }
}

// 处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);

        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == conn)
            {
                // 从map表删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    // 更新用户的状态信息
    if (user.getId() != -1)
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
}

// 一对一聊天服务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int perrID = js["toid"].get<int>(); // toid

    // 1、本地服务器toid在线：在map表上找toid
    {
        lock_guard<mutex> lock(_connMutex);

        auto it = _userConnMap.find(perrID);
        if (it != _userConnMap.end())
        {
            // toid在线，转发消息:服务器主动推送消息给toid用户
            it->second->send(js.dump());
            return;
        }
    }

    // 2、其他服务器toid在线：
    User user = _userModel.query(perrID);
    if (user.getState() == "online")
    {
        _redis.publish(perrID, js.dump());
        return;
    }

    // 3、toid不在线，存储离线消息
    _offlineMsgModel.insert(perrID, js.dump());
}

// 处理服务端异常退出
void ChatService::reset()
{
    // 把online状态的用户，设置成offline
    _userModel.resetState();
}

// 添加好友业务 msgid friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    // 存储好友信息
    _friendModel.insert(userid, friendid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string groupname = js["groupname"];
    string groupdesc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, groupname, groupdesc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    _groupModel.addGroup(userid, groupid, "normal");
}

// 群聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();

    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    {
        lock_guard<mutex> lock(_connMutex);
        for (int id : useridVec)
        {
            auto it = _userConnMap.find(id);
            // 1、本地服务器id在线：在map表上找id
            if (it != _userConnMap.end())
            {
                // 转发群消息
                it->second->send(js.dump());
            }
            else
            {
                // 2、其他服务器id在线：
                User user = _userModel.query(id);
                if (user.getState() == "online")
                {
                    _redis.publish(id, js.dump());
                    return;
                }
                // 3、id不在线
                else
                {
                    // 存储离线群消息
                    _offlineMsgModel.insert(id, js.dump());
                }
            }
        }
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    // 更新用户的状态信息
    User user;
    user.setId(userid);
    _userModel.updateState(user);
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    json js = json::parse(msg.c_str());

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    // 1、在线
    if (it != _userConnMap.end())
    {
        it->second->send(js.dump());
        return;
    }

    // 2、离线：存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}
