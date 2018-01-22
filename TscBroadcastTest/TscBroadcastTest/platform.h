#pragma once
#if defined(_MSC_VER)
#include <windows.h>
#include <intrin.h>
#define CACHE_ALIGN(x) \
__declspec(align(64)) x

inline bool SetThreadAffinity(size_t CpuId)
{
    size_t CpuGroup = CpuId / (sizeof(size_t) * 8);
    size_t CpuNum = CpuId % (sizeof(size_t) * 8);
    GROUP_AFFINITY affinity = { 1ull << CpuNum, CpuGroup };
    if (!SetThreadGroupAffinity(GetCurrentThread(), &affinity, NULL))
    {
        return false;
    }
    return true;
}

#else

#define CACHE_ALIGN(x) \
x __attribute((alligned(64)))

#include <pthread.h>
inline bool SetThreadAffinity(size_t CpuId)
{
    cpu_set_t cpuset;
    pthread_t thread;
    thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(CpuId, &cpuset);
    if (0 != pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset))
    {
        return false;
    }
    CPU_ZERO(&cpuset);
    if (0 != pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset) ||
        (!CPU_ISSET(CpuId, &cpuset)))
    {
        return false;
    }
    return true;
}

#endif
