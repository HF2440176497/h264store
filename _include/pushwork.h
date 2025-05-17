
#ifndef _PUSHWORK_H_
#define _PUSHWORK_H_

#include <memory>
#include <cstdio>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>

#include "encoder.h"
#include "frame_queue.h"


class PushWork {
public:
    PushWork(int queue_size, int width, int height);
    ~PushWork();

public:
    int init();  // 开启线程
    bool put_data(cv::Mat mat);
    void stop(int timeout_seconds);
    void set_finish();

private:
    void consumer_thread();
    void init_params();


private:
    int fps = 25;
    std::mutex mtx_;  // 线程运行标志位的互斥量

protected:
    std::thread worker_;
    std::atomic<bool> running{true};           // 线程运行标志位
    std::atomic<bool> has_finished_{false};    // 线程是否运行结束
    std::condition_variable cv_;
    Encoder encoder_;

private:
    FrameQueue<cv::Mat> queue_;
    int queue_size;
};

#endif