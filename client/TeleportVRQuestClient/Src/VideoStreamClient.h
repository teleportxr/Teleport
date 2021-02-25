// (C) Copyright 2018 Simul.co

#pragma once

#include <atomic>
#include <thread>
#include <string>

#include <libavstream/libavstream.hpp>

class VideoStreamClient
{
public:
    VideoStreamClient(avs::Queue* recvQueue);
    ~VideoStreamClient();

    bool StartReceiving(const std::string& address, uint16_t port);
    void StopReceiving();

private:
    void RecvThreadMain(std::string address, uint16_t port);

    avs::Queue* mRecvQueue;
    std::thread mRecvThread;
    std::atomic<bool> mIsReceiving;
};
