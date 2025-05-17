/**
* @brief         decode h264 file, and save to BGR
* @author        wanghf
* @date          2025.05.15
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <iostream>
#include <string>


#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/highgui.hpp>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>

#ifdef __cplusplus
}
#endif


#define VIDEO_INBUF_SIZE 1280000
#define VIDEO_REFILL_THRESH 320000

// 图像尺寸
const int width = 2432;
const int height = 2048;

static char err_buf[128] = {0};
static char* av_get_err(int errnum)
{
    av_strerror(errnum, err_buf, 128);
    return err_buf;
}

static void print_video_format(const AVFrame *frame)
{
    printf("width: %u\n", frame->width);
    printf("height: %u\n", frame->height);
    printf("format: %u\n", frame->format);  // 格式需要注意 应当是 YUV420P
}


static int64_t get_timestamp_ms() {
    auto now = std::chrono::high_resolution_clock::now();  // 获取当前时刻的高精度时间点
    auto duration_since_epoch = now.time_since_epoch();    // 转换为自纪元（1970-01-01 00:00:00 UTC）以来的毫秒数
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration_since_epoch);
    return milliseconds.count();  // 返回毫秒数
}


int save_frame_file(AVFrame* save_frame, uint8_t* buf) {
    int ret = 0;
    cv::Mat cv_mat(save_frame->height, save_frame->width, CV_8UC3, buf);
    cv_mat = cv_mat.clone();
    int rows = cv_mat.rows;
    int cols = cv_mat.cols;
    int channels = cv_mat.channels();
    std::cout << "Image shape: (" << rows << ", " << cols << ", " << channels << ")" << std::endl;

    std::string file_name = "images/" + std::to_string(get_timestamp_ms()) + ".png";
    bool result = cv::imwrite(file_name, cv_mat);
    if (!result) {
        std::cerr << "image save error" << std::endl;
        return -1;
    }
    return ret;
}


static void decode(AVCodecContext *dec_ctx, AVPacket *pkt, AVFrame *decoded_frame, 
    AVFrame *save_frame, SwsContext* sws_ctx_, uint8_t* buf)
{
    int ret;
    /* send the packet with the compressed data to the decoder */
    ret = avcodec_send_packet(dec_ctx, pkt);
    if(ret == AVERROR(EAGAIN))
    {
        fprintf(stderr, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
    }
    else if (ret < 0)
    {
        fprintf(stderr, "Error submitting the packet to the decoder, err:%s, pkt_size:%d\n",
                av_get_err(ret), pkt->size);
        return;
    }

    /* read all the output frames (infile general there may be any number of them */
    while (ret >= 0)
    {
        // 对于frame, avcodec_receive_frame内部每次都先调用
        ret = avcodec_receive_frame(dec_ctx, decoded_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0)
        {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }
        // 打印
        static int s_print_format = 0;
        if(s_print_format == 0)
        {
            s_print_format = 1;
            print_video_format(decoded_frame);
        }
        // 获取转换器
        sws_ctx_ = sws_getCachedContext(
            sws_ctx_,
            width, height, AV_PIX_FMT_YUV420P,
            width, height, AV_PIX_FMT_BGR24,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        // height: 输入图像的高度
        ret = sws_scale(sws_ctx_, decoded_frame->data, decoded_frame->linesize, 0, height, save_frame->data, save_frame->linesize);
        if (ret < 0) {
            fprintf(stderr, "sws_scale failed\n");
            exit(1);
        }
        save_frame_file(save_frame, buf);
    }
}


/**
 * 初始化待保存图像帧
 */
int save_frame_init(AVFrame* save_frame, uint8_t* buf) {
    int ret = 0;
    if (!save_frame) {
        if (!(save_frame = av_frame_alloc())) {
            fprintf(stderr, "Could not allocate audio frame\n");
            exit(1);
        }
    }
    save_frame->width = width;
    save_frame->height = height;
    save_frame->format = AV_PIX_FMT_BGR24;  // 目标格式
    auto dst_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
    buf = (uint8_t*)av_malloc(dst_size *sizeof(uint8_t));
    if (!buf) {
        fprintf(stderr, "Could not malloc frame space\n");
        exit(1);
    }
    av_image_fill_arrays(save_frame->data, save_frame->linesize, buf, 
        AV_PIX_FMT_BGR24, width, height, 1);
    return ret;
}


int main(int argc, char **argv)
{
    const AVCodec *codec;
    AVCodecContext *codec_ctx= NULL;
    AVCodecParserContext *parser = NULL;
    int len = 0;
    int ret = 0;

    const char *filename;
    FILE *infile = NULL;

    // AV_INPUT_BUFFER_PADDING_SIZE 在输入比特流结尾的要求附加分配字节的数量上进行解码
    uint8_t inbuf[VIDEO_INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data = NULL;
    size_t   data_size = 0;
    AVPacket *pkt = NULL;
    AVFrame *decoded_frame = NULL;
    SwsContext* sws_ctx = NULL;
    AVFrame *save_frame = NULL;  // 转换后的图像帧
    uint8_t *buf = nullptr;  // 待转换图像帧的对应空间

    // if (argc <= 1)
    // {
    //     fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
    //     exit(0);
    // }
    filename = "out.h264";  // 待解码的 h264 文件

    pkt = av_packet_alloc();
    enum AVCodecID video_codec_id = AV_CODEC_ID_H264;
    if(strstr(filename, "264") != NULL)
    {
        video_codec_id = AV_CODEC_ID_H264;
    }
    else if(strstr(filename, "mpeg2") != NULL)
    {
        video_codec_id = AV_CODEC_ID_MPEG2VIDEO;
    }
    else
    {
        printf("default codec id:%d\n", video_codec_id);
    }

    // 查找解码器
    codec = avcodec_find_decoder(video_codec_id);  // AV_CODEC_ID_H264
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }
    parser = av_parser_init(codec->id);
    if (!parser) {
        fprintf(stderr, "Parser not found\n");
        exit(1);
    }
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        exit(1);
    }
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        exit(1);
    }
    infile = fopen(filename, "rb");
    if (!infile) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }
    // 初始化转换帧
    // save_frame_init(save_frame, buf);

    if (!save_frame) {
        if (!(save_frame = av_frame_alloc())) {
            fprintf(stderr, "Could not allocate audio frame\n");
            exit(1);
        }
    }
    save_frame->width = width;
    save_frame->height = height;
    save_frame->format = AV_PIX_FMT_BGR24;  // 目标格式
    auto dst_size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
    buf = (uint8_t*)av_malloc(dst_size *sizeof(uint8_t));
    if (!buf) {
        fprintf(stderr, "Could not malloc frame space\n");
        exit(1);
    }
    av_image_fill_arrays(save_frame->data, save_frame->linesize, buf, 
        AV_PIX_FMT_BGR24, width, height, 1);


    // 读取文件进行解码
    data      = inbuf;
    data_size = fread(inbuf, 1, VIDEO_INBUF_SIZE, infile);

    while (data_size > 0)
    {
        if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc()))
            {
                fprintf(stderr, "Could not allocate audio frame\n");
                exit(1);
            }
        }
        ret = av_parser_parse2(parser, codec_ctx, &pkt->data, &pkt->size,
                               data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0)
        {
            fprintf(stderr, "Error while parsing\n");
            exit(1);
        }
        data      += ret;   // 跳过已经解析的数据
        data_size -= ret;   // 对应的缓存大小也做相应减小

        if (pkt->size)
            decode(codec_ctx, pkt, decoded_frame, save_frame, sws_ctx, buf);

        if (data_size < VIDEO_REFILL_THRESH)    // 如果数据少了则再次读取
        {
            memmove(inbuf, data, data_size);    // 把之前剩的数据拷贝到buffer的起始位置
            data = inbuf;
            // 读取数据 长度: VIDEO_INBUF_SIZE - data_size
            len = fread(data + data_size, 1, VIDEO_INBUF_SIZE - data_size, infile);
            if (len > 0)
                data_size += len;
        }
    }  // end while (data_size > 0)

    // 冲刷解码器
    pkt->data = NULL;
    pkt->size = 0;
    decode(codec_ctx, pkt, decoded_frame, save_frame, sws_ctx, buf);

    fclose(infile);

    avcodec_free_context(&codec_ctx);
    av_parser_close(parser);
    sws_freeContext(sws_ctx);
    av_frame_free(&decoded_frame);

    av_free(buf);  // 先释放内存空间
    av_frame_free(&save_frame);
    av_packet_free(&pkt);

    printf("main finish, please enter Enter and exit\n");
    return 0;
}
