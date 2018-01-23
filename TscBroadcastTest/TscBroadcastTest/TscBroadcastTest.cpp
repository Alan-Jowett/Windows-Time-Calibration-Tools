// Simple tool to measure the TSC offset between two CPU cores.
// Reports the offset as mean, median and stdev, along with the round trip time of the measure.

#include "stdafx.h"
#include <atomic>
#include <thread>
#include <vector>
#include <algorithm>

volatile bool stop = false;

void CollectSamples(std::atomic<bool> & SignalLocal, std::atomic<bool> & SignalRemote, bool Client, std::vector<unsigned long long> & Samples)
{
    unsigned int i;
    for (size_t index = 0; index < Samples.size(); index++)
    {
        while (!stop && SignalLocal.load() != Client)
        {
        }
        unsigned long long ts = __rdtscp(&i);
        Samples[index] = ts;
        SignalLocal.store(!Client);
        SignalRemote.store(!Client);
    }
    stop = true;
}

void ComputeStats(std::vector<long long> Samples, long long & Mean, long long & Median, long long & StdDev)
{
    Mean = 0;
    Median = 0;
    StdDev = 0;
    std::sort(Samples.begin(), Samples.end());
    std::for_each(Samples.begin(), Samples.end(), [&](long long Sample)
    {
        Mean += Sample;
    });
    Mean /= static_cast<long long>(Samples.size());
    std::for_each(Samples.begin(), Samples.end(), [&](long long Sample) 
    {
        StdDev += (Sample - Mean) * (Sample - Mean);
    });
    StdDev /= static_cast<long long>(Samples.size());
    StdDev = std::sqrt(StdDev);
    Median = Samples[Samples.size() / 2];
}

void * AllocPageForProc(size_t CpuId)
{
    size_t CpuGroup = CpuId / (sizeof(size_t) * 8);
    size_t CpuNum = CpuId % (sizeof(size_t) * 8);

    PROCESSOR_NUMBER pn = { CpuGroup, CpuNum };
    USHORT numaNode = 0;
    if (!GetNumaProcessorNodeEx(&pn, &numaNode))
    {
        return nullptr;
    }
    return VirtualAllocExNuma(GetCurrentProcess(), nullptr, 4096, MEM_COMMIT, PAGE_READWRITE, numaNode);
}

int main(int argc, char ** argv)
{
    if (argc != 4)
    {
        printf("Usage: %s cpu# cpu# iterations\n", argv[0]);
        printf("Example: %s 0 1 1000000\n", argv[0]);
        exit(-1);
    }

    size_t serverCpu = atoi(argv[1]);
    size_t clientCpu = atoi(argv[2]);
    size_t samples = atoi(argv[3]);
    std::vector<unsigned long long> tsClient;
    std::vector<unsigned long long> tsServer;
    void * clientPage = AllocPageForProc(clientCpu);
    void * serverPage = AllocPageForProc(serverCpu);

    std::atomic<bool> * signalClient = new (clientPage) std::atomic<bool>(false);
    std::atomic<bool> * signalServer = new (serverPage) std::atomic<bool>(false);

    printf("O-Mean\tO-Med\tO-STDEV\tR-Mean\tR-Med\tR-STDEV\n");
    for (size_t i = 0; i < 10; i++)
    {
        std::atomic<bool> & clientOwns = *signalClient;
        std::atomic<bool> & serverOwns = *signalServer;
        clientOwns.store(false);
        serverOwns.store(false);
        stop = false;
        // Client and server are arbitrary
        auto client = std::thread([&tsClient, &clientOwns, &serverOwns, samples, clientCpu]() {
            if (!SetThreadAffinity(clientCpu))
            {
                printf("Failed to set CPU affinity");
                exit(-1);
            }
            tsClient.resize(samples);
            CollectSamples(clientOwns, serverOwns, true, tsClient);
        });
        auto server = std::thread([&tsServer, &clientOwns, &serverOwns, samples, serverCpu]() {
            if (!SetThreadAffinity(serverCpu)) {
                printf("Failed to set CPU affinity");
                exit(-1);
            }
            tsServer.resize(samples);
            CollectSamples(serverOwns, clientOwns, false, tsServer);
        });
        client.join();
        server.join();

        std::vector<long long> offsets;
        std::vector<long long> rtts;
        long long avgOffset = 0;
        for (size_t i = 0; i < samples - 1; i++)
        {
            // If the TSC was synchronized, then tsClient[i] would be half way betweem tsServer[i] and tsServer[i+1]
            long long offset = (2 * (long long)tsClient[i] - (long long)tsServer[i] - (long long)tsServer[i + 1]) / 2;
            long long rtt = (long long)tsServer[i + 1] - (long long)tsServer[i];
            offsets.push_back(offset);
            rtts.push_back(rtt);
        }

        long long mean, median, stddev;
        ComputeStats(offsets, mean, median, stddev);
        printf("%lld\t%lld\t%lld\t", mean, median, stddev);
        ComputeStats(rtts, mean, median, stddev);
        printf("%lld\t%lld\t%lld\n", mean, median, stddev);
    }
}