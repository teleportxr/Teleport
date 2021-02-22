// (C) Copyright 2018-2019 Simul Software Ltd

#include "VideoStreamClient.h"

#include <OVR_LogUtils.h>
#include <sched.h>
#include <chrono>

static constexpr auto StatInterval = std::chrono::seconds(1);

VideoStreamClient::VideoStreamClient(avs::Queue *recvQueue)
    : mRecvQueue(recvQueue)
    , mIsReceiving(false)
{
    assert(mRecvQueue);
}

VideoStreamClient::~VideoStreamClient()
{
    StopReceiving();
}

bool VideoStreamClient::StartReceiving(const std::string &address, uint16_t port)
{
    if(mIsReceiving.load()) {
        OVR_WARN("VideoStreamClient: Already receiving");
        return false;
    }

    mIsReceiving.store(true);
    mRecvThread = std::thread(&VideoStreamClient::RecvThreadMain, this, address, port);
    return true;
}

void VideoStreamClient::StopReceiving()
{
    mIsReceiving.store(false);
    if(mRecvThread.joinable())
    {
        mRecvThread.join();
    }
}

void VideoStreamClient::RecvThreadMain(std::string address, uint16_t port)
{
    avs::NetworkSourceParams params = {};
    params.remoteIP = address.c_str(); // 16MiB socket buffer size
    params.connectionTimeout = 90000;
    params.localPort = port + 1;
    params.remotePort = port;
    //params.gcTTL = (1000/60) * 4; // TTL = 4 * expected frame time

    avs::NetworkSource networkSource;
    if(!networkSource.configure({},params))
    {
        OVR_WARN("VideoStreamClient: Failed to configure network source");
        return;
    }

    avs::Forwarder forwarder;
    forwarder.configure(1, 1, 64 * 1024);

    avs::Pipeline pipeline;
    pipeline.link( { &networkSource, &forwarder, mRecvQueue} );

    auto lastTimestamp = std::chrono::steady_clock::now();
    while(mIsReceiving.load())
    {
        pipeline.process();
        sched_yield();

        const auto timestamp = std::chrono::steady_clock::now();
        if(timestamp - lastTimestamp >= StatInterval)
        {
            const avs::NetworkSourceCounters counters = networkSource.getCounterValues();
            OVR_WARN("NP: %lu/%lu | DP: %lu/%lu | BYTES: %lu",
                     (unsigned long)counters.networkPacketsReceived, (unsigned long)counters.networkPacketsDropped,
                     (unsigned long)counters.decoderPacketsReceived, (unsigned long)counters.decoderPacketsDropped,
                     (unsigned long)counters.bytesReceived
            );
            lastTimestamp = timestamp;
        }
    }
}
