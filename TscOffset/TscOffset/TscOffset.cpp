// TscOffset.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <intrin.h>
#include <thread>
#include <atomic>
#include <Windows.h>

struct Message {
    unsigned long long ClientTx;
    unsigned long long ClientRx;
    unsigned long long ServerRx;
    unsigned long long ServerTx;
};

void Server(size_t CpuId, Message * msg)
{
    //SetThreadAffinity(CpuId);
    unsigned long long oldClientTx = 0;
    for (;;)
    {
        unsigned int i;
        unsigned long long ts = __rdtscp(&i);
        unsigned long long currentClientTx = msg->ClientTx;
        if (currentClientTx == oldClientTx)
        {
            continue;
        }
        oldClientTx = currentClientTx;
        msg->ServerRx = ts;
        std::atomic_thread_fence(std::memory_order_release);
        msg->ServerTx = __rdtscp(&i);
    }
}

void Client(size_t CpuId, Message * msg)
{
    //SetThreadAffinity(CpuId);
    unsigned long long oldServerTx = 0;
    unsigned int i;
    unsigned long long tx = __rdtscp(&i);
    msg->ClientTx = tx;
    for (;;)
    {
        unsigned int i;
        unsigned long long currentServerTx = msg->ServerTx;
        if (currentServerTx == oldServerTx)
        {
            continue;
        }
        
        msg->ClientRx = __rdtscp(&i);
        break;
    }
}

int main()
{
    Message m = { 0 };
    std::thread t([&]() { Server(0, &m);  });

    Client(1, &m);
    exit(0);
    return 0;
}

