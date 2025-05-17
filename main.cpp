
#include <string>

#include "pushwork.h"



int main() {

    std::string image_path = "./data";
    int queue_size = 10;

    PushWork worker(image_path, queue_size, 2432, 2048);
    worker.init();
    worker.test03();
    
    int timeout_seconds = 3;
    worker.stop(timeout_seconds);  // 等待线程终止
    
    return 0;
}

