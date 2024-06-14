/**
 * 测试epoll惊群
 * 编译服务器：g++ -o epoll epoll.cpp  -lpthread
 * 客户端使用telnet连接测试：telnet 127.0.0.1 8888
 * 这里会出现epoll的惊群现象，也就是在epoll_wait被触发时
 * 多个线程都会被唤醒
 * 解决惊群的一种方法：
 * 在linux4.5内核之后给epoll添加了一个 EPOLLEXCLUSIVE的标志位，
 * 如果设置了这个标志位，那epoll将进程挂到等待队列时将会设置一下互斥标志位，
 * 这时实现跟内核原生accept一样的特性，只会唤醒队列中的一个进程
*/
#include <netinet/in.h>
#include <iostream>
#include <sys/epoll.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

#define WORKER_THREAD 4

//创建socket
int createSocket()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        std::cerr << "Failed to create socket." << std::endl;
        return -1;
    }

    int reuse_addr = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0) {
        std::cerr << "setsockopt() failed" << std::endl;
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_port = htons(8888);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);

    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        cout << "Failed to bind socket." << endl;
        return -1;
    }

    if (listen(fd, 100) < 0)
    {
        cout << "Failed to listen socket." << endl;
        return -1;
    }
    
    return fd;
}

void Worker2(int socketFd, int k)
{
    cout << "worker " << k << " run " << endl;
    int eFd = epoll_create(11111);
    if (eFd < 0)
    {
        cout << "epoll_create failed" << endl;
        return;
    }
    
    epoll_event event = {};
    event.events = EPOLLIN;
    //给epoll加上 互斥标志
    // event.events = EPOLLIN | EPOLLEXCLUSIVE;

    event.data.fd = socketFd;
    if (epoll_ctl(eFd, EPOLL_CTL_ADD, socketFd, &event) < 0)
    {
        cout << "epoll_ctl failed" << endl;
        return;
    }

    epoll_event events[1024];

    while (true)
    {
        int eNum = epoll_wait(eFd, events, 1024, -1);
        if (eNum < 0)
        {
            cout << "epoll_wait failed" << endl;
            return;
        }

        //等待1s，防止看不到结果
        sleep(1);

        cout << "worker " << k << " in " << endl;
        for (int i = 0; i < eNum; i++)
        {
            //新连接事件
            if (events[i].data.fd == socketFd)
            {
                sockaddr_in addr = {};
                socklen_t len = sizeof(addr);
                int cfd = accept(socketFd, (sockaddr*)&addr, &len);
                if (cfd <= 0)
                {
                    cout << "accept failed" << endl;
                }
                else
                {
                    cout << "worker " << k << " accept" << endl;
                }
                
            }
            else
            {
                //socket读写事件
            }
        }
    }
}

int main()
{
    std::mutex mutex;
    std::unique_lock<std::mutex> lck(mutex);
    std::condition_variable cv;

    int socketFd = createSocket();
    if (socketFd < 0)
    {
        return -1;
    }

    for (int i = 0; i < WORKER_THREAD; i++)
    {
        sleep(1);
        std::thread th(Worker2, socketFd, i + 1);
        th.detach();
    }

    cv.wait(lck);
    return 0;
}