/* 
 * File:   mp4_encoder.h
 * Copyright : Ethic Consultant @2014
 * Author: hungnv
 *
 * Created on December 19, 2014, 1:26 AM
 */

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>

//AVFormatContext *ifmt_ctx;
//AVFormatContext *ofmt_ctx;

typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;
//FilteringContext *filter_ctx;


static int open_input_file(const char *filename, AVFormatContext *ifmt_ctx);
static int open_output_file(const char *filename, int width, int height,
        AVFormatContext *ofmt_ctx, AVFormatContext *ifmt_ctx);
static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
        AVCodecContext *enc_ctx, const char *filter_spec);
static int init_filters(int width, int height, AVFormatContext *ifmt_ctx, 
         AVFormatContext *ofmt_ctx, FilteringContext *filter_ctx);
static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame,
        AVFormatContext *ofmt_ctx, AVFormatContext *ifmt_ctx);
static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index, 
        FilteringContext *filter_ctx, AVFormatContext *ofmt_ctx, AVFormatContext *ifmt_ctx);
static int flush_encoder(unsigned int stream_index, AVFormatContext *ofmt_ctx, 
        AVFormatContext *ifmt_ctx);
int convert(const char *input, const char* output);


