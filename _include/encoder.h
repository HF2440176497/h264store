
#ifndef _ENCODER_H_
#define _ENCODER_H_



#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <filesystem>
#include <mutex>
#include <condition_variable>

// opencv 相关头文件
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/highgui.hpp>


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}


class Encoder {
public:
    Encoder(int width, int height);
    ~Encoder();

public:
    int init();
    int frame_process(const cv::Mat& mat);
    void encode_end();

private:
    int alloc_input_frame();
    int alloc_push_frame();
    int codec_init();
    int init_convert();  // 创建转换器对象
    int update_output_file();
    int encode_write(AVFrame* p_frame = nullptr);
    int encode_call();

private:
    static void validate_frame_and_mat(const cv::Mat& mat, const AVFrame* frame);
    static void validate_pixel_format(const cv::Mat& mat, AVPixelFormat format);


public:
    int width_ = 2432;
    int height_ = 2048;

private:
    AVFrame* frame_in = nullptr;  // 转换前的图像帧
    AVFrame* push_frame = nullptr;  // 转换后的图像帧
    AVPacket* pkt = nullptr;  // 
    SwsContext* sws_ctx_ = nullptr;
    const AVCodec *codec = nullptr;
    AVCodecContext *codec_ctx = nullptr;

private:
    int64_t pts = 0;  // 时间戳
    int fps_ = 10;  // 编码视频帧率
    uint64_t frame_count = 0;  // 帧计数变量
    static const int FRAMES_PER_FILE = 30;  // 单个编码文件的图像帧数目
    
    FILE* output_file_ = nullptr;  // 当前编码输出文件
    bool initialized_ = false;
};


#endif