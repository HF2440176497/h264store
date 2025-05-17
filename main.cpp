
#include <string>

#include "pushwork.h"



std::vector<std::filesystem::path> load_images(std::string image_path_) {
    std::vector<std::filesystem::path> images_;
    for (const auto& entry : std::filesystem::directory_iterator(image_path_)) {
        if (entry.is_regular_file()) {
            const std::string ext = entry.path().extension().string();
            if (ext == ".jpg" || ext == ".png" || ext == ".bmp") {
                images_.push_back(entry.path());
            }
        }
    }  // end for
    return images_;
}


void print_files(std::string image_path_) {
    for (const auto& entry : std::filesystem::directory_iterator(image_path_)) {
        if (entry.is_regular_file()) {
            const std::string cur_file = entry.path().filename();
            std::cout << "cur_file: " << cur_file << std::endl;
        }
    }
}

int main() {
    int queue_size = 10;

    PushWork worker(queue_size, 2432, 2048);
    worker.init();

    std::string image_path = "./data";
    auto images = load_images(image_path);
    print_files(image_path);

    // 测试代码
    {
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
            worker.put_data(cv_mat);
            currentIndex = (currentIndex + 1) % images.size();
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    int timeout_seconds = 3;
    worker.stop(timeout_seconds);  // 等待线程终止
    
    return 0;
}

