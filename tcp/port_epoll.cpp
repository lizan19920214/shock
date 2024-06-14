/**
 * 解决epoll惊群的一种方法
 * 编译服务器：g++ -o port_epoll port_epoll.cpp  -lpthread
 * 客户端使用telnet连接测试：telnet 127.0.0.1 8888
 * 解决惊群的另一种方法：
 * 让每个线程分别打开一个socket，并且这些socket绑定在同一个端口，
 * 然后accept这个socket。这就像第一种情景那样，内核直接帮我们做了惊群处理。
 * 这里会使用到 linux 3.9后 socket提供SO_REUSEPORT标志。使用这个标志后，会允许多个socket绑定和监听同一个端口
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
    if (fd < 0)
    {
        cout << "Failed to create socket." << endl;
        return -1;
    }

    int reuser_addr = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuser_addr, sizeof(int));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuser_addr, sizeof(int));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    addr.sin_addr.s_addr = htons(INADDR_ANY);

    if (bind(fd, (sockaddr*) &addr, sizeof(addr)) < 0)
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

void Worker(int k)
{
    cout << "worker " << k << " run " << endl;

    int socketFd = createSocket();
    int eFd = epoll_create(1);
    if (eFd == -1)
    {
        cout << "Failed to create epoll." << endl;
        return;
    }

    epoll_event event = {};
    event.events = EPOLLIN;
    event.data.fd = socketFd;

    epoll_ctl(eFd, EPOLL_CTL_ADD, socketFd, &event);

    epoll_event events[1024];

    while (true)
    {
        int eNum = epoll_wait(eFd, events, 1024, -1);
        if (eNum == -1)
        {
            cout << "Failed to wait epoll." << endl;
            return;
        }

        //等待1s，防止看不到结果
        sleep(1);
        cout << " worker " << k << " in " << endl;

        for (int i = 0; i < eNum; i++)
        {
            //处理新连接
            if (events[i].data.fd == socketFd)
            {
                sockaddr_in cli_addr = {};
                socklen_t len = sizeof(cli_addr);
                int tfd = accept(socketFd, (sockaddr*)&cli_addr, &len);
                if (tfd <= 0)
                {
                    cout << "accept error" << endl;
                }
                else
                {
                    cout << " worker " << k << " accept " << endl;
                }
                
            }
            else
            {
                //处理读写
                
            }
            
        }
    }
}

int main()
{
    std::mutex mutex;
    std::unique_lock<std::mutex> lck(mutex);
    std::condition_variable cv;

    for (int i = 0; i < WORKER_THREAD; i++)
    {
        sleep(1);
        std::thread t(Worker, i + 1);
        t.detach();
    }

    cv.wait(lck);
    return 0;
}