// TscOffset.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <intrin.h>
#include <thread>
#include <atomic>
#include <Windows.h>

volatile struct Message {
    enum eState {
        Idle = 0,
        ClientPrep = 1,
        ClientDone = 2,
        ServerPrep = 3,
        ServerDone = 4
    };

    __declspec(align(64)) struct {
        long long T1;
        long long T4;
    };
    __declspec(align(64)) struct {
        long long T2;
        long long T3;
    };
    __declspec(align(64))struct {
        unsigned long State;
    };
} Msg = {};

void Server(size_t CpuId)
{
    DWORD_PTR mask = 1 << CpuId;
    SetThreadAffinityMask(GetCurrentThread(), mask);
    unsigned int i;
    for (;;)
    {
        if (InterlockedCompareExchange(&Msg.State, Message::ServerPrep, Message::ClientDone) != Message::ClientDone)
        {
            continue;
        }
        Msg.T2 = __rdtscp(&i);
        Msg.T3 = __rdtscp(&i);
        InterlockedExchange(&Msg.State, Message::ServerDone);
    }
}

void Client(size_t CpuId, long RttBound, long long & Offset, long long & Rtt)
{
    DWORD_PTR mask = 1 << CpuId;

    SetThreadAffinityMask(GetCurrentThread(), mask);
    unsigned int i;
    for (;;)
    {
        if (InterlockedCompareExchange(&Msg.State, Message::ClientPrep, Message::Idle) != Message::Idle)
        {
            continue;
        }
        Msg.T1 = __rdtscp(&i);
        InterlockedExchange(&Msg.State, Message::ClientDone);
        while (InterlockedCompareExchange(&Msg.State, Message::Idle, Message::ServerDone) != Message::ServerDone)
        {
        }
        Msg.T4 = __rdtscp(&i);

        Offset = ((Msg.T2 - Msg.T1) + (Msg.T3 - Msg.T4)) / 2;
        Rtt = (Msg.T4 - Msg.T1) - (Msg.T3 - Msg.T2);

        if (Msg.T2 > Msg.T3)
        {
            continue;
        }

        if (Msg.T1 > Msg.T4)
        {
            continue;
        }

        if (Rtt > RttBound)
        {
            continue;
        }
        break;
    }
}

int main(int argc, char ** argv)
{ 
    volatile Message m = { 0 };
    int server = atoi(argv[1]);
    int client = atoi(argv[2]);
    int iterations = atoi(argv[3]);
    int rttBounds = atoi(argv[4]);
    std::thread t([&]() { Server(server);  });
    for (size_t i = 0; i < 10; i++)
    {
        long long averageOffset = 0;
        long long averageRtt = 0;
        
        for (size_t j = 0; j < iterations; j++)
        {
            long long offset = 0;
            long long rtt = 0;
            Client(client, rttBounds, offset, rtt);
            averageOffset += offset;
            averageRtt += rtt;
        }
        averageOffset /= iterations;
        averageRtt /= iterations;
        printf("%lld\t%lld\n", averageOffset, averageRtt);
    }
    exit(0);
    return 0;
}

