

#include "encoder.h"
#include "utils.h"


static void print_frame_info(const AVFrame* frame) {
    if (!frame) {
        std::cerr << "Frame is NULL" << std::endl;
        return;
    }
    std::cout << "Frame Information:" << std::endl;
    std::cout << "  Format: " << frame->format << std::endl;
    std::cout << "  Width: " << frame->width << std::endl;
    std::cout << "  Height: " << frame->height << std::endl;
}


Encoder::Encoder(int width, int height) : width_(width), height_(height) {
}


Encoder::~Encoder() {
    if (frame_in) av_frame_free(&frame_in);
    if (push_frame) av_frame_free(&push_frame);
    if (sws_ctx_) sws_freeContext(sws_ctx_);
    if (pkt) av_packet_free(&pkt);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
}


/**
 * 编码器相关初始化工作
 */
int Encoder::init() {
    int ret = 0;
    pkt = av_packet_alloc();
    if (!pkt) {
        std::cerr << "Could not allocate video packet" << std::endl;
        ret = -1;
    }
    if (alloc_input_frame() < 0 || alloc_push_frame() < 0) {
        std::cerr << "Could not allocate video frame" << std::endl;
        ret = -1;
    }
    if ((ret = codec_init()) < 0) {
        std::cerr << "Could not initialize encoder" << std::endl;
    }
    return ret;
}


/**
 * 分配输入图片对应的图像帧
 */
int Encoder::alloc_input_frame() {
    int ret;
    frame_in = av_frame_alloc();
    if (!frame_in) {
        std::cerr << "frame_in av_frame_alloc error" << std::endl;
        return -1;
    }
    frame_in->format = AV_PIX_FMT_BGR24;
    frame_in->width = width_;
    frame_in->height = height_;
    ret = av_image_alloc(frame_in->data, frame_in->linesize, width_, height_, AV_PIX_FMT_BGR24, 1);
    if (ret < 0) {
        std::cerr << "input_frame alloc failed" << std::endl;
        av_frame_free(&frame_in);
    }
    return ret;
}


/**
 * 分配待推流的图像帧
 */
int Encoder::alloc_push_frame() {
    this->push_frame = av_frame_alloc();
    if (!(this->push_frame)) {
        std::cerr << "push_frame av_frame_alloc error" << std::endl;
    }
    this->push_frame->format = AV_PIX_FMT_YUV420P;
    this->push_frame->width = width_;
    this->push_frame->height = height_;

    int ret = -1;
    if ((ret = av_frame_get_buffer(this->push_frame, 1)) != 0) {
        std::cerr << "av_frame_get_buffer error" << std::endl;
        av_frame_free(&push_frame);
    }
    return ret;
}


int Encoder::codec_init() {
    int ret = 0;
    codec = const_cast<AVCodec*>(avcodec_find_encoder(AV_CODEC_ID_H264));
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate video codec context\n");
        return -1;
    }

    this->codec_ctx->codec_id = this->codec->id;
    this->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;

    this->codec_ctx->width = width_;
    this->codec_ctx->height = height_;
    this->codec_ctx->time_base = (AVRational){1, fps_};
    this->codec_ctx->framerate = (AVRational){fps_, 1};

    this->codec_ctx->gop_size = 5;                // 设置为帧数
    this->codec_ctx->max_b_frames = 0;              // B 帧数目
    this->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;  // 编码的图像格式

    // 设置压缩等相关指标
    if (codec->id == AV_CODEC_ID_H264) {
        ret = av_opt_set(codec_ctx->priv_data, "preset", "medium", 0);
        if (ret != 0) {
            printf("av_opt_set preset failed\n");
        }
        ret = av_opt_set(codec_ctx->priv_data, "profile", "main", 0);
        if (ret != 0) {
            printf("av_opt_set profile failed\n");
        }
    }
    // 绑定编码器
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "avcodec_open2 could not open codec: %d\n", ret);
        return ret;
    }
    return ret;
}


void Encoder::validate_frame_and_mat(const cv::Mat& mat, const AVFrame* frame) {
    // 检查宽度和高度
    if (mat.cols != frame->width || mat.rows != frame->height) {
        throw std::runtime_error(
            "shape not match: cv::Mat(" + std::to_string(mat.cols) + "x" + std::to_string(mat.rows) + 
            ") vs AVFrame(" + std::to_string(frame->width) + "x" + std::to_string(frame->height) + ")"
        );
    }
    // 检查通道数
    int mat_channels = mat.channels();
    int frame_channels;
    switch (frame->format) {
        case AV_PIX_FMT_GRAY8:     frame_channels = 1; break;  // 灰度
        case AV_PIX_FMT_BGR24:     frame_channels = 3; break;  // OpenCV 默认 BGR
        case AV_PIX_FMT_RGB24:     frame_channels = 3; break;
        case AV_PIX_FMT_YUV420P:   frame_channels = 1; break;  // YUV 平面格式
        default:
            throw std::runtime_error("not support format of input frame");
    }
    if (mat_channels != frame_channels) {
        throw std::runtime_error(
            "channels num not match: cv::Mat(" + std::to_string(mat_channels) + 
            ") vs AVFrame(" + std::to_string(frame_channels) + ")"
        );
    }
}


void Encoder::validate_pixel_format(const cv::Mat& mat, AVPixelFormat format) {
    bool is_compatible = false;
    switch (mat.type()) {
        case CV_8UC1:
            is_compatible = (format == AV_PIX_FMT_GRAY8);
            break;
        case CV_8UC3:
            is_compatible = (format == AV_PIX_FMT_BGR24 || format == AV_PIX_FMT_RGB24);
            break;
        case CV_8UC4:
            is_compatible = (format == AV_PIX_FMT_BGRA || format == AV_PIX_FMT_RGBA);
            break;
    }
    if (!is_compatible) {
        throw std::runtime_error("pix format not match");
    }
}


/**
 * 返回此帧的处理结果
 */
int Encoder::frame_process(const cv::Mat& mat) {
    int ret = 0;
    if (!frame_in) {
        std::cerr << "frame_in not valid " << std::endl;
        return -1;
    }
    validate_frame_and_mat(mat, frame_in);
    validate_pixel_format(mat, (AVPixelFormat)frame_in->format);
    
    ret = av_image_fill_arrays(frame_in->data, frame_in->linesize, 
                mat.data, AV_PIX_FMT_BGR24, mat.cols, mat.rows, 1);
    if (ret < 0) {
        std::cerr << "frame_in fill data failed" << std::endl;
        return ret;
    }
    if ((ret = init_convert()) < 0) {
        std::cerr << "sws_scale failed: " << ret << "; frame_process exit"<< std::endl;
        return ret;
    }
    ret = sws_scale(sws_ctx_, frame_in->data, frame_in->linesize, 0, height_, push_frame->data, push_frame->linesize);
    if (ret < 0) {
        std::cerr << "sws_scale failed: " << ret << "; frame_process exit"<< std::endl;
        return ret;
    }
    ret = encode_call();  // 开始编码 push_frame
    return ret;
}

void Encoder::encode_end() {
    if (output_file_) {
        encode_write();
        fclose(output_file_);
    }
}

int Encoder::init_convert() {
    int ret = 0;
    sws_ctx_ = sws_getCachedContext(
        this->sws_ctx_,
        width_, height_, AV_PIX_FMT_BGR24,
        width_, height_, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!sws_ctx_) {
        std::cerr << "sws_ctx_ not valid" << std::endl;
        return -1;
    }
    return ret;
}

/**
 * p_frame == NULL 冲刷编码器
 */
int Encoder::encode_write(AVFrame* p_frame) {
    int ret;

    if (p_frame) {
        p_frame->pts = pts;
        pts += 1;
    }
    ret = avcodec_send_frame(codec_ctx, p_frame);
    if (ret < 0) {
        fprintf(stderr, "Error sending a frame for encoding\n");
        return -1;
    }
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error encoding audio frame\n");
            return -1;
        }
        fwrite(pkt->data, 1, pkt->size, output_file_);
    }
    return 0;
}


/**
 * 更新创建的输出文件
 */
int Encoder::update_output_file() {
    if (output_file_) {
        fclose(output_file_);
        output_file_ = nullptr;
    }
    // 可选择其他命名策略
    std::string filename = std::to_string(get_time_ms()) + ".h264";
    output_file_ = fopen(filename.c_str(), "wb");
    if (!output_file_) {
        fprintf(stderr, "Could not open output file %s\n", filename.c_str());
        return -1;
    }
    return 0;
}


int Encoder::encode_call() {
    int ret = 0;
    if (frame_count % FRAMES_PER_FILE == 0) {
        if (update_output_file() < 0) {
            return -1;
        }
        avcodec_flush_buffers(codec_ctx);
    }
    ret = encode_write(push_frame);
    if (ret < 0) {
        std::cerr << "encode_write failed, encode_call exit" << std::endl;
        return ret;
    }
    if (push_frame) {
        frame_count++;
    }
    return ret;
}