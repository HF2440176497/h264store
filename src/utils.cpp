
#include "utils.h"


int64_t get_time_ms()  {
    auto now = std::chrono::high_resolution_clock::now();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return milliseconds;
}
