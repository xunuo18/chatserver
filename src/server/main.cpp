#include "chatserver.hpp"
#include "chatservice.hpp"
#include <signal.h>
using namespace std;

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }

    //解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 信号注册函数：绑定「收到Ctrl+C中断信号时要执行的处理函数」
    // 1.SIGINT = 键盘按下Ctrl+C时，操作系统自动发给当前进程的中断信号
    // 2.resetHandler:自定义的信号处理函数
    signal(SIGINT, resetHandler);

    EventLoop loop;
    InetAddress addr(ip, port);

    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();

    return 0;
}