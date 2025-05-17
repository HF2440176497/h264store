#include <string>

#include "pushwork.h"
#include "frame_queue.h"
#include "utils.h"


PushWork::PushWork(std::string image_path, int queue_size, int width, int height) : 
                image_path_(image_path), queue_(queue_size),
                encoder_(width, height) {
    load_images();
}


PushWork::~PushWork() {
    int timeout_seconds = 3;
    if (!has_finished_.load()) {  // 如果线程没有停止
        this->stop(timeout_seconds);
    } 
}


void PushWork::load_images() {
    for (const auto& entry : std::filesystem::directory_iterator(image_path_)) {
        if (entry.is_regular_file()) {
            const std::string ext = entry.path().extension().string();
            if (ext == ".jpg" || ext == ".png" || ext == ".bmp") {
                images.push_back(entry.path());
            }
        }
    }  // end for
}


void PushWork::print_files() {
    for (const auto& entry : std::filesystem::directory_iterator(image_path_)) {
        if (entry.is_regular_file()) {
            const std::string cur_file = entry.path().filename();
            std::cout << "cur_file: " << cur_file << std::endl;
        }
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


void PushWork::test01() {
    for (const auto& img_path : images) {
        cv::Mat cv_mat = cv::imread(img_path.string(), cv::IMREAD_COLOR);
        if (cv_mat.empty()) {
            std::cerr << "Warning: Failed to read " << img_path << std::endl;
            continue;
        }
        encoder_.frame_process(cv_mat);
    }
}

// 模拟功能: 以一定间隔取出图片处理
void PushWork::test02() {
    size_t currentIndex = 0;
    auto startTime = std::chrono::steady_clock::now();
    int displayTimeInSeconds = 60;

    while (true) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        if (elapsedTime > displayTimeInSeconds) {
            break;
        }
        auto path = images[currentIndex];  // 当前图片

        cv::Mat cv_mat = cv::imread(path.string(), cv::IMREAD_COLOR);
        if (cv_mat.empty()) {
            std::cerr << "Warning: Failed to read " << path << std::endl;
            continue;
        }
        auto start_time = get_time_ms();
        encoder_.frame_process(cv_mat);
        auto end_time = get_time_ms();
        printf("frame process time cost: %ld ms\n", end_time - start_time);

        currentIndex = (currentIndex + 1) % images.size();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

}

// 模拟功能: 交给队列, 作为生产者
void PushWork::test03() {
    size_t currentIndex = 0;
    auto startTime = std::chrono::steady_clock::now();
    int displayTimeInSeconds = 60;

    while (true) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        if (elapsedTime > displayTimeInSeconds) {
            break;
        }
        auto path = images[currentIndex];  // 当前图片
        cv::Mat cv_mat = cv::imread(path.string(), cv::IMREAD_COLOR);
        if (cv_mat.empty()) {
            std::cerr << "Warning: Failed to read " << path << std::endl;
            continue;
        }
        put_data(cv_mat);
        currentIndex = (currentIndex + 1) % images.size();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

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
    this->print_files();
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