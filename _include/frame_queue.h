
#ifndef _FRAMEQUEUE_H_
#define _FRAMEQUEUE_H_

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>


template<typename T>
struct PopResult {
    std::optional<T> item;
    bool is_stopped;
    PopResult(std::optional<T> _item, bool _is_stopped) : item(_item), is_stopped(_is_stopped) {}
};


/**
 * 线程安全队列
 */
template<typename T>
class FrameQueue {

public:
    FrameQueue(int max_size) : max_size_(max_size) {
        if (max_size <= 0) {
            throw std::invalid_argument("max_size must be greater than 0");
        }
    }
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /**
     * 添加元素
     * timeout = -1 表示无限等待
     */
     bool push(const T& item, int timeout_ms = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 等待队列未满或停止信号
        auto wait_predicate = [this] { 
            return !stop_ && queue_.size() < max_size_; 
        };
        if (!cond_var_.wait_for(lock, get_timeout(timeout_ms), wait_predicate)) {
            return false;
        }
        queue_.push(item);
        cond_var_.notify_one();  // 通知一个等待消费者
        return true;
    }

    /**
     * 阻塞取出队列元素
     */
    PopResult<T> pop(int timeout_ms = -1) {  // -1表示无限等待
        std::unique_lock<std::mutex> lock(mutex_);
        
        auto timeout = (timeout_ms == -1) ? 
                      std::chrono::milliseconds::max() : 
                      std::chrono::milliseconds(timeout_ms);

        if (!cond_var_.wait_for(lock, timeout, [this] { return !queue_.empty() || stop_; })) {
            return PopResult<T>(std::nullopt, stop_);
        }
        // 可能非空或终止
        if (stop_) {
            return PopResult<T>(std::nullopt, true);
        }
        T item = queue_.front();
        queue_.pop();
        return PopResult<T>(std::make_optional(item), false);
    }

    /**
     * 非阻塞尝试获取队列元素 并不检查队列是否终止
     */
    PopResult<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return PopResult<T>(std::nullopt, stop_);
        T item = queue_.front();
        queue_.pop();
        return PopResult<T>(std::make_optional(item), stop_);
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cond_var_.notify_all();
    }

    /**
     * 查询最大容量
     */
    size_t capacity() const {
        return max_size_;
    }

private:
    std::chrono::milliseconds get_timeout(int timeout_ms) const {
        return (timeout_ms == -1) ? 
            std::chrono::milliseconds::max() : 
            std::chrono::milliseconds(timeout_ms);
    }


private:
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
    std::queue<T> queue_;
    bool stop_ = false;  // 停止标志
    int max_size_ = 10;  // 默认大小

};


#endif