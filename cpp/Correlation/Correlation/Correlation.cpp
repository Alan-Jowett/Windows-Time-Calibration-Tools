// Correlation.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <time.h>

#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

struct Sample {
    unsigned long long LocalStart;
    unsigned long long LocalEnd;
    unsigned long long Local;
    unsigned long long RemoteStart;
    unsigned long long RemoteEnd;
    unsigned long long Remote;
};

std::vector<std::string> Split(const std::string& String, const char Delimiter)
{
    std::vector<std::string> tokens;
    std::vector<size_t> offsets;
    size_t next = 0;
    offsets.push_back(next);
    while (next != std::string::npos)
    {
        next = String.find_first_of(Delimiter, next + 1);
        offsets.push_back(next);
    }
    tokens.push_back(String.substr(0, offsets[1]));
    for (size_t i = 1; i < offsets.size() - 1; i++)
    {
        size_t start = offsets[i] + 1;
        size_t length = offsets[i + 1] - start;
        tokens.push_back(String.substr(start, length));
    }
    return tokens;
}

std::vector<Sample> ParseSamples(const char * FileName)
{
    std::vector<Sample> samples;
    std::ifstream samplesFile(FileName, std::ifstream::in);

    while (samplesFile.good())
    {
        Sample sample{};
        std::string line;
        std::getline(samplesFile, line);
        auto tokens = Split(line, ',');
        switch (tokens.size())
        {
        case 0:
        case 1:
            continue;
            break;

        case 2:
            sample.LocalEnd = sample.LocalStart = atoll(tokens[0].c_str());
            sample.RemoteStart = sample.RemoteEnd = atoll(tokens[1].c_str());
            break;

        case 3:
            sample.LocalEnd = atoll(tokens[0].c_str());
            sample.LocalStart = atoll(tokens[1].c_str());
            sample.RemoteStart = sample.RemoteEnd = atoll(tokens[2].c_str());
            break;

        case 4:
            sample.LocalEnd = atoll(tokens[0].c_str());
            sample.LocalStart = atoll(tokens[1].c_str());
            sample.RemoteStart = atoll(tokens[2].c_str());
            sample.RemoteEnd = atoll(tokens[3].c_str());
        default:
            break;
        }
        sample.Local = sample.LocalEnd / 2 + sample.LocalStart / 2;
        sample.Remote = sample.RemoteEnd / 2 + sample.RemoteStart / 2;
        samples.push_back(sample);
    }
    return samples;
}

size_t FindSamples(const Sample & RefSample, const std::vector<Sample> & TestSamples)
{
    size_t low = 0;
    size_t high = TestSamples.size() - 1;
    size_t Offset = high / 2 + low / 2;;

    while ((high - low) > 1)
    {
        Offset = (high + low) / 2;
        if (RefSample.Local > TestSamples[Offset].Local)
        {
            low = Offset;
        }
        else
        {
            high = Offset;
        }
    }
    return Offset;
}

unsigned long long InterpolateSample(size_t RefOffset, const std::vector<Sample> & RefSamples, unsigned long long Point, size_t Depth)
{
    double y = 0;
    double x = (double)Point;
    for (size_t i = RefOffset - Depth / 2; i <= RefOffset + Depth / 2; i++)
    {
        double c = 1;
        for (size_t j = RefOffset - Depth / 2; j <= RefOffset + Depth / 2; j++)
        {
            if (i == j)
            {
                continue;
            }

            c *= (x - (double)RefSamples[j].Local) / ((double)RefSamples[i].Local - (double)RefSamples[j].Local);
        }
        y += (double)RefSamples[i].Remote * c;
    }
    return (unsigned long long)y;
}


bool NtFileTimeToLocalTime(unsigned long long TimeStamp, tm & Time)
{
    time_t posixTime = (unsigned)(TimeStamp / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
#ifdef _MSC_VER
    return (localtime_s(&Time, &posixTime) == 0);
#else
    Time = *localtime(&posixTime);
    return true;
#endif
}


std::vector<Sample> BiasSamples(unsigned long long Local, unsigned long long Remote, const std::vector<Sample> & Samples)
{
    std::vector<Sample> biasedSamples;
    for (const auto & sample : Samples)
    {
        biasedSamples.push_back({ sample.LocalStart - Local, sample.LocalEnd - Local, sample.Local - Local, sample.RemoteStart - Remote, sample.RemoteEnd, sample.Remote - Remote });
    }
    return biasedSamples;
}


int main(int argc, const char ** argv)
{
    if (argc == 1)
    {
        std::cerr << "Usage: " << argv[0] << " Reference.csv Test.csv offset" << std::endl;
        return -1;
    }

    std::vector<Sample> referenceSamples = ParseSamples(argv[1]);
    std::vector<Sample> testSamples = ParseSamples(argv[2]);

    unsigned long long localBias = std::min(referenceSamples[0].LocalStart, testSamples[0].LocalStart);
    unsigned long long remoteBias = std::min(referenceSamples[0].RemoteStart, testSamples[0].RemoteStart);
    std::vector<Sample> biasedReferenceSamples = BiasSamples(localBias, remoteBias, referenceSamples);
    std::vector<Sample> biasedTestSamples = BiasSamples(localBias, remoteBias, testSamples);

    for (size_t testOffset = 2; testOffset < biasedTestSamples.size() - 2; testOffset++)
    {
        size_t refOffset = FindSamples(biasedTestSamples[testOffset], biasedReferenceSamples);
        if (refOffset < 3 || refOffset + 3 > biasedReferenceSamples.size())
        {
            continue;
        }
        long long offset = biasedTestSamples[testOffset].Remote - InterpolateSample(refOffset, biasedReferenceSamples, biasedTestSamples[testOffset].Local, 5);
        long long rtt = InterpolateSample(refOffset, biasedReferenceSamples, biasedTestSamples[testOffset].LocalEnd, 5) - InterpolateSample(refOffset, biasedReferenceSamples, biasedTestSamples[testOffset].LocalStart, 5);
        tm time;
        NtFileTimeToLocalTime(InterpolateSample(refOffset, referenceSamples, testSamples[testOffset].Local, 5), time);
        std::cout << std::put_time(&time, "%c") << "," << (double)offset / 10 << "," << (double)rtt / 10 << std::endl;


    }

    return 0;
}

