#include "db.h"
#include <muduo/base/Logging.h> //包含打印日志

// 数据库配置信息
static string server = "127.0.0.1"; // MySQL服务端IP
static string user = "chatapp";              // 登录MySQL的用户名
static string password = "123456";        // 密码
static string dbname = "chat";            // 连接后默认打开的数据库

// 初始化数据库资源 - 未建立连接
MySQL::MySQL()
{
    _conn = mysql_init(nullptr); // 向MySQL库申请一个连接实例,赋值给_conn
}
// 释放数据库连接资源
MySQL::~MySQL()
{
    if (_conn != nullptr)
    {
        mysql_close(_conn);
    }
}
// 连接数据库
bool MySQL::connect()
{
    // 参数：连接实例、IP、账号、密码、库名、端口 3306、unix 套接字为空、标志位 0
    // 返回值：p非空 = 连接成功、空指针 = 连接失败
    // c_str():
    // 1、所调函数为MySQ原生C接口，函数参数只接收const char*
    // 2、需要把C++风格的string字符串，转换成C语言标准的const char*字符数组指,末尾自带\0
    // 3、返回的是const char*：不能通过这个指针修改字符串内容
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if (p != nullptr)
    {
        // mysql_query()：通过已建立的连接，把SQL字符串发送给MySQL服务器执行
        // c/C++代码默认的编码字符是ASCII，如果不设置，从MysQL上拉下来的中文显示?，乱码
        // 发送一条设置字符集的SQL，告诉MySQL客户端用gbk编码收发数据
        // 注意：现在MySQL普遍用utf8mb4，写gbk会导致中文乱码，建议改为set names utf8mb4
        mysql_query(_conn, "set names utf8mb4");
        LOG_INFO << "connect mysql success!";
    }
    else
    {
        LOG_ERROR << "connect mysql fail, errno="
                  << mysql_errno(_conn)
                  << ", error=" << mysql_error(_conn);
    }
    return p;
}
// 更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        // 出错时用LOG_INFO打印：__FILE__当前文件名、__LINE__出错行号、出错SQL语句，方便调试
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败！";
        return false;
    }
    return true;
}
// 查询操作
MYSQL_RES *MySQL::query(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败！";
        return nullptr;
    }
    // mysql_use_result:
    // 1、获取结果集句柄MYSQL_RES*
    // 2、后续外部代码用mysql_fetch_row逐行读取数据
    // 3、外部读取结束后必须手动mysql_free_result释放结果集，否则内存泄漏
    return mysql_use_result(_conn);
}
// 获取连接
MYSQL *MySQL::getConnection()
{
    return _conn;
}
