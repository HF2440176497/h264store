
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
    PushWork(std::string image_path, int queue_size, int width, int height);
    ~PushWork();

public:
    void test01();
    void test02();
    void test03();

public:
    int init();  // 开启线程
    void stop(int timeout_seconds);
    void set_finish();

private:
    bool put_data(cv::Mat mat);
    void consumer_thread();
    void init_params();
    void load_images();
    void print_files();


private:
    std::string image_path_;
    std::vector<std::filesystem::path> images;  // 图片路径列表
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