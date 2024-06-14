/**
 * 测试accept在多线程下的惊群问题
 * 编译服务器：g++ -o accept accept.cpp  -lpthread
 * 客户端使用telnet连接测试：telnet 127.0.0.1 8888
 * accept的惊群问题在Linux2.6之后就不会发生了
 * 因为在Linux 2.6 版本之后，通过引入一个标记位 WQ_FLAG_EXCLUSIVE，解决掉了 Accept 惊群效应
*/
#include <netinet/in.h>
#include <iostream>
#include <sys/epoll.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>

using namespace std;

#define WORKER_THREAD 4

//创建socket，并返回fd
int createSocket()
{
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
    {
        cout << "create socket error" << endl;
        return -1;
    }
    
    sockaddr_in addr = {};
    addr.sin_port = htons(8888);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        cout << "bind error" << endl;
        return -1;
    }

    if (listen(fd, 100) < 0)
    {
        cout << "listen error" << endl;
        return -1;
    }

    return fd;
}

void Worker1(int socketFd, int k)
{
    cout << " worker " << k << " run" << endl;
    while(true)
    {
        int clientFd = 0;
        sockaddr_in cli_addr = {};
        socklen_t len = sizeof(cli_addr);
        clientFd = accept(socketFd, (sockaddr *)&cli_addr, &len);
       if (clientFd <= 0)
       {
            cout << "worker " << k << " accept error " << endl;
            return;
       }
       else
       {
            cout << "worker " << k << " accept client " << clientFd << endl;
       }
    }
}

int main()
{
    std::mutex mutex;
    std::unique_lock<std::mutex> lck(mutex);
    std::condition_variable cv;

    int fd = createSocket();
    if (fd < 0)
    {
        cout << "create socket error" << endl;
        return -1;
    }

    //多线程监听fd
    for (int i = 0; i < WORKER_THREAD; i++)
    {
        sleep(2);
        std::thread th(&Worker1, fd, i + 1);
        th.detach();
    }
    
    cv.wait(lck);

    return 0;
}