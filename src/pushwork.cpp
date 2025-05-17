#include <string>

#include "pushwork.h"
#include "frame_queue.h"
#include "utils.h"


PushWork::PushWork(int queue_size, int width, int height) : 
                queue_(queue_size),
                encoder_(width, height) {
}


PushWork::~PushWork() {
    int timeout_seconds = 3;
    if (!has_finished_.load()) {  // 如果线程没有停止
        this->stop(timeout_seconds);
    } 
}


/**
 * 开启线程
 */
int PushWork::init() {
    int ret = 0;
    ret = encoder_.init();
    if (ret < 0) {
        return ret;
    }
    running = true;
    worker_ = std::thread(&PushWork::consumer_thread, this);
    return ret;  
}


void PushWork::stop(int timeout_seconds) {
    running = false;
    queue_.stop();
    std::cerr << "PushWork prepare to stop" << std::endl;
    bool status;
    {
        std::unique_lock<std::mutex> lck(mtx_);
        auto timeout = std::chrono::seconds(timeout_seconds);
        status = cv_.wait_for(lck, timeout, [this] { 
            return has_finished_.load();
        });
    }
    // 谓词判断为真 条件变量返回
    if (has_finished_.load() && worker_.joinable()) {
        worker_.join();
    } else {
        std::cerr << "PushWork stop status: " << status << std::endl;
    }
}


/**
 * 暴露给 Python 的接口
 */
bool PushWork::put_data(cv::Mat mat) {
    bool ret = queue_.push(mat);
    std::cout << "push ret: " << ret << "; queue size: " << queue_.size() << std::endl;
    return ret;
}


/**
 * 线程运行起始时的初始化
 */
void PushWork::init_params() {
}


/**
 * 线程函数
 */
void PushWork::consumer_thread() {
    int ret = 0;  // 线程内运行结果反馈
    init_params();
    while (running && ret >= 0) {
        PopResult<cv::Mat> res = queue_.pop();
        auto item = res.item;
        bool is_queue_stop = res.is_stopped;
        if (!item.has_value()) {
            if (is_queue_stop) {  // 进一步判断是否是队列终止
                ret = -1;
                continue;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));  // 队列为空，继续等待
            continue;
        }
        try {
            cv::Mat mat = item.value();

            auto start_time = get_time_ms();
            encoder_.frame_process(mat);
            auto end_time = get_time_ms();
            printf("frame process time cost: %ld ms\n", end_time - start_time);
        } catch(const std::exception& e) {
            std::cout << "consumer_thread process error: " << e.what() << std::endl;
        }
    }
    encoder_.encode_end();
    set_finish();
    std::cout << "consumer_thread end" << std::endl;
}


void PushWork::set_finish() {
    {
        std::lock_guard<std::mutex> lck(mtx_);
        has_finished_ = true;
    }
    cv_.notify_all();
}

/**
 * 可能会存在获取不到的情况
 */