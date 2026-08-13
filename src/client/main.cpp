// 客户端不需要高并发，采用原始的基于Linux的TCP客户端编程
#include "json.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <ctime>
using namespace std;
using json = nlohmann::json;

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <atomic>

#include "Group.hpp"
#include "user.hpp"
#include "public.hpp"

// 记录当前系统登录的用户信息
User g_currentUser;
// 记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
// 记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;

// 控制主菜单页面程序
bool isMainMenuRunning = false;

// 用于读写线程之间的通信
sem_t rwsem;
// 记录登录状态
atomic_bool g_isLoginSuccess{false};

// 接收线程
void readTaskHandler(int clientfd);
// 获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime();
// 主聊天页面程序
void mainMenu(int clientfd);
// 显示当前登录成功用户的基本信息
void showCurrentUserData();

// 聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
// argc:命令行参数总个数(包含程序自身名称)
// argv：存储所有命令行参数字符串
// ① char *：指向一个C风格字符串，数组里每个元素是char*（单个参数字符串）
// ② char **：argv本身指向这个char*类型的数组首地址
int main(int argc, char **argv)
{
    // 1.判断发送命令个数
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatClient 192.168.230.135 6000" << endl;
        exit(-1);
    }

    // 2.解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 3.创建client端的通信socket
    int cfd = socket(AF_INET, SOCK_STREAM, 0); // 地址族：IPv4，协议类型：流式，参数3=0：TCP协议
    if (cfd == -1)
    {
        cerr << "socket create error" << endl;
        close(cfd);
        exit(-1);
    }

    // 4.填写client需要连接的server信息ip+port
    sockaddr_in saddr{0};
    saddr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &saddr.sin_addr.s_addr);
    saddr.sin_port = htons(port);

    // 5.client和server进行连接
    int ret = connect(cfd, (sockaddr *)&saddr, sizeof(sockaddr_in));
    if (ret == -1)
    {
        cerr << "server connect error" << endl;
        close(cfd);
        exit(-1);
    }

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0); // 参数2：线程间通信-0、参数3：初始值0

    // 连接服务器成功，启动接收子线程
    std::thread readTask(readTaskHandler, cfd);
    readTask.detach();

    // 6.main线程用于接收用户输入，负责发送数据
    while (1)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "=================================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "=================================" << endl;
        cout << "choice:";
        int choice = 0;
        cin >> choice;
        cin.get(); // 读掉缓冲区残留的回车

        switch (choice)
        {
        case 1: // login业务
        {
            int id = 0;
            char pwd[50] = {0};
            cout << "input user id:";
            cin >> id;
            cin.get();
            cout << "input user password:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = id;
            js["password"] = pwd;
            string request = js.dump(); // 序列化

            g_isLoginSuccess = false;

            int len = send(cfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len < 0)
            {
                // send失败
                cerr << "send login msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，由子线程处理完登录的响应消息后，通知这里

            if (g_isLoginSuccess)
            {
                // 进入聊天主菜单页面
                isMainMenuRunning = true;
                mainMenu(cfd);
            }

            break;
        }

        case 2: // register业务
        {
            // ① cin >> name：空格、回车就结束读取；\n会残留在缓冲区，不会被>>丢弃
            // ② cin.getline(name,50)：按下回车键才结束读取，支持读取空格；换行符\n会被getline自动从缓冲区丢弃
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "input user name:";
            cin.getline(name, 50);
            cout << "input user password:";
            cin.getline(pwd, 50);

            json js;
            js["msgid"] = REG_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump(); // 序列化

            int len = send(cfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len < 0)
            {
                // send失败
                cerr << "send reg msg error:" << request << endl;
            }

            sem_wait(&rwsem); // 等待信号量，由子线程处理完注册的响应消息后，通知这里
            break;
        }

        case 3: // quit业务
            close(cfd);
            sem_destroy(&rwsem);
            exit(0);
        default:
            cerr << "invalid input!" << endl;
            break;
        }
    }

    return 0;
}

// 处理登录的响应逻辑
void doLoginResponse(json &responsejs)
{
    if (responsejs["errno"].get<int>() != 0)
    {
        // 登录失败
        cout << responsejs["errmsg"] << endl;
        g_isLoginSuccess = false;
    }
    else
    {
        // 登录成功
        // 记录当前用户的id和name
        g_currentUser.setId(responsejs["id"].get<int>());
        g_currentUser.setName(responsejs["name"]);
        // 记录当前用户的好友列表信息
        if (responsejs.find("friends") != responsejs.end())
        {
            g_currentUserFriendList.clear();
            vector<string> vec_f = responsejs["friends"];
            for (string &str : vec_f)
            {
                json js = json::parse(str);
                User user;
                user.setId(js["id"].get<int>());
                user.setName(js["name"]);
                user.setState(js["state"]);
                g_currentUserFriendList.push_back(user);
            }
        }

        // 记录当前用户的群组列表信息
        if (responsejs.find("groups") != responsejs.end())
        {
            g_currentUserGroupList.clear();
            vector<string> vec_g = responsejs["groups"];
            for (string &str_g : vec_g)
            {
                json js_g = json::parse(str_g);
                Group group;
                group.setId(js_g["id"].get<int>());
                group.setName(js_g["groupname"]);
                group.setDesc(js_g["groupdesc"]);

                vector<string> vec_u = js_g["users"];
                for (string &str_u : vec_u)
                {
                    json js_u = json::parse(str_u);
                    GroupUser user;
                    user.setId(js_u["id"].get<int>());
                    user.setName(js_u["name"]);
                    user.setState(js_u["state"]);
                    user.setRole(js_u["role"]);
                    group.getUsers().push_back(user);
                }

                g_currentUserGroupList.push_back(group);
            }
        }

        // 显示用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息：各人聊天消息或群组消息
        if (responsejs.find("offlinemsg") != responsejs.end())
        {
            vector<string> vec_o = responsejs["offlinemsg"];
            for (string &str : vec_o)
            {
                json js = json::parse(str);
                // time [id] name said: msg
                int msgtype = js["msgid"].get<int>();
                if (msgtype == ONE_CHAT_MSG)
                {
                    cout << js["time"].get<string>() << " [" << js["id"] << "] " << js["name"].get<string>()
                         << " said: " << js["msg"].get<string>() << endl;
                }
                else if (msgtype == GROUP_CHAT_MSG)
                {
                    cout << "群消息[" << js["groupid"] << "] " << js["time"].get<string>() << " [" << js["id"]
                         << "] " << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }
        g_isLoginSuccess = true;
    }
}

// 处理注册的响应逻辑
void doRegResponse(json responsejs)
{
    if (responsejs["errno"].get<int>() != 0)
    {
        // 注册失败
        cerr << "name is already exist, register error!" << endl;
    }
    else
    {
        // 注册成功 - 返回注册id
        cout << "name register success, userid is " << responsejs["id"] << ", do not forget it!" << endl;
    }
}

// 接收线程
void readTaskHandler(int clientfd)
{
    while (1)
    {
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if (len <= 0)
        {
            close(clientfd);
            exit(-1);
        }

        // 接收chatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        if (msgtype == ONE_CHAT_MSG)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "] " << js["name"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
        else if (msgtype == GROUP_CHAT_MSG)
        {
            cout << "群消息[" << js["groupid"] << "] " << js["time"].get<string>() << " [" << js["id"]
                 << "] " << js["name"].get<string>() << " said: " << js["msg"].get<string>() << endl;
            continue;
        }
        else if (msgtype == LOGIN_MSG_ACK)
        {
            doLoginResponse(js); // 处理登录响应的业务逻辑
            sem_post(&rwsem);    // 通知主线程，登录结果处理完成
            continue;
        }
        else if (msgtype == REG_MSG_ACK)
        {
            doRegResponse(js); // 处理注册响应的业务逻辑
            sem_post(&rwsem);  // 通知主线程，注册结果处理完成
            continue;
        }
    }
}

//"help" command handler
void help(int fd = 0, string str = "");
//"chat" command handler
void chat(int cfd, string str);
//"addfriend" command handler
void addfriend(int cfd, string str);
//"creategroup" command handler
void creategroup(int cfd, string str);
//"addgroup" command handler
void addgroup(int cfd, string str);
//"groupchat" command handler
void groupchat(int cfd, string str);
//"loginout" command handler
void loginout(int cfd, string str);

// 系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令,格式help"},
    {"chat", "一对一聊天,格式chat:friendid:message"},
    {"addfriend", "添加好友,格式addfriend:friendid"},
    {"creategroup", "创建群组,格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组,addgroup:groupid"},
    {"groupchat", "群聊,格式groupchat:groupid:message"},
    {"loginout", "注销,格式loginout"}};

// 注册系统支持的客户端命令处理 - int:cfd string:命令
unordered_map<string, function<void(int, string)>> commandHandlerMap = {
    {"help", help},
    {"chat", chat},               // 聊天
    {"addfriend", addfriend},     // 添加好友
    {"creategroup", creategroup}, // 建群组
    {"addgroup", addgroup},       // 添加群组
    {"groupchat", groupchat},     // 群组聊天
    {"loginout", loginout}};      // 用户注销

// 主聊天页面程序
void mainMenu(int clientfd)
{
    help();

    char buffer[1024] = {0};
    while (isMainMenuRunning)
    {
        cin.getline(buffer, 1024);
        string commandbuff(buffer);
        string command; // 存储命令
        int idx = commandbuff.find(":");
        if (idx == -1)
        {
            command = commandbuff;
        }
        else
        {
            command = commandbuff.substr(0, idx);
        }
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }
        // 调用相应命令的事件处理回调，mainMenu对修改封闭，添加新功能不需要修改该函数
        it->second(clientfd, commandbuff.substr(idx + 1, commandbuff.size() - idx)); // 调用命令处理方法
    }
}

//"help" command handler
void help(int cfd, string str)
{
    cout << "show command list >>> " << endl;
    for (auto &p : commandMap)
    {
        cout << p.first << " : " << p.second << endl;
    }
    cout << endl;
}

//"chat" command handler  friendid:message
void chat(int cfd, string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        cerr << "chat command invaild!" << endl;
        return;
    }

    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();

    string buffer = js.dump();

    int len = send(cfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len < 0)
    {
        cerr << "send chat msg error -> " << buffer << endl;
    }
}

//"addfriend" command handler  friendid
void addfriend(int cfd, string str)
{
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();

    int len = send(cfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len < 0)
    {
        cerr << "send addfriend msg error -> " << buffer << endl;
    }
}
//"creategroup" command handler  groupname:groupdesc
void creategroup(int cfd, string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        cerr << "creategroup command invaild!" << endl;
        return;
    }

    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(cfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len < 0)
    {
        cerr << "send creategroup msg error -> " << buffer << endl;
    }
}
//"addgroup" command handler groupid
void addgroup(int cfd, string str)
{
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;
    string buffer = js.dump();

    int len = send(cfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len < 0)
    {
        cerr << "send addgroup msg error -> " << buffer << endl;
    }
}
//"groupchat" command handler  groupid:message
void groupchat(int cfd, string str)
{
    int idx = str.find(":");
    if (idx == -1)
    {
        cerr << "groupchat command invaild!" << endl;
        return;
    }

    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.size() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();

    string buffer = js.dump();

    int len = send(cfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len < 0)
    {
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}
//"loginout" command handler
void loginout(int cfd, string str)
{
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(cfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len < 0)
    {
        cerr << "send loginout msg error -> " << buffer << endl;
    }
    else
    {
        isMainMenuRunning = false;
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData()
{
    cout << "=====================login user=====================" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << " name:" << g_currentUser.getName() << endl;
    cout << "---------------------friend list--------------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (User &user : g_currentUserFriendList)
        {
            cout << user.getId() << " " << user.getName() << " " << user.getState() << endl;
        }
    }
    cout << "---------------friend list show finish--------------" << endl;
    cout << "---------------------group list---------------------" << endl;
    if (!g_currentUserGroupList.empty())
    {
        for (Group &group : g_currentUserGroupList)
        {
            cout << group.getId() << " " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState() << " " << user.getRole() << endl;
            }
        }
    }
    cout << "---------------group list show finish---------------" << endl;
    cout << "====================================================" << endl;
}

// 获取系统时间(聊天信息需要添加时间信息)
string getCurrentTime()
{
    // 获取当前系统时间
    auto now = chrono::system_clock::now();
    time_t nowTime = chrono::system_clock::to_time_t(now);

    // 转换为本地时间
    tm localTime{};
    localtime_r(&nowTime, &localTime);

    // 格式化时间
    char buffer[32] = {0};
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);

    return string(buffer);
}