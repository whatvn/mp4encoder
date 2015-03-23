/* 
 * File:   mp4_encoder.c
 * Copyright : Ethic Consultant @2014
 * Author: hungnv
 */


#include "mp4_encoder.h"

static int open_input_file(const char *filename, AVFormatContext *ifmt_ctx) {
    int ret;
    unsigned int i;
    int res;
    //    ifmt_ctx = NULL;
    AVInputFormat* input_format = NULL;
//    av_find_input_format()
//    input_format = av_find_input_format(filename);
//    if (input_format != NULL) printf("format file input: %s\n", input_format->name);
    if ((ret = avformat_open_input(&ifmt_ctx, filename, input_format, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream;
        AVCodecContext *codec_ctx;
        stream = ifmt_ctx->streams[i];
        codec_ctx = stream->codec;
        int has_audio = 0, has_video = 0;
        /* Reencode video & audio and remux subtitles etc. */
        if ((codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
                || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) && (has_video == 0 || has_audio == 0)) {
            /* Open decoder */
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (codec_ctx->width >= 1080) {
                    res = 1; //720p
                } else if (codec_ctx->width >= 854) {
                    res = 2; //480p
                } else {
                    res = 3; //360p
                }
                has_video = 1;
            } else {
                has_audio = 1;
            }
            ret = avcodec_open2(codec_ctx,
                    avcodec_find_decoder(codec_ctx->codec_id), NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
        }
    }

    av_dump_format(ifmt_ctx, 0, filename, 0);
    return res;
}

static int open_output_file(const char *filename, int width, int height,
        AVFormatContext *ofmt_ctx, AVFormatContext *ifmt_ctx) {
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    AVDictionary *option = NULL;
    unsigned int i;
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }

    int has_audio = 0, has_video = 0;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        enum AVMediaType type = ifmt_ctx->streams[i]->codec->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO || type == AVMEDIA_TYPE_AUDIO || type == AVMEDIA_TYPE_SUBTITLE) {
            out_stream = avformat_new_stream(ofmt_ctx, NULL);
            if (!out_stream) {
                av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
                return AVERROR_UNKNOWN;
            }

            in_stream = ifmt_ctx->streams[i];
            dec_ctx = in_stream->codec;
            enc_ctx = out_stream->codec;
        } else {
            continue;
        }
        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
                || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO && has_video == 0) {
                encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
                if (!encoder) {
                    av_log(NULL, AV_LOG_FATAL, "Necessary Video encoder not found\n");
                    return AVERROR_INVALIDDATA;
                }
                has_video = 1;
                enc_ctx->height = height;
                enc_ctx->width = width;
                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
                /* take first format from list of supported formats */
                enc_ctx->pix_fmt = encoder->pix_fmts[0];
                /* video time_base can be set to whatever is handy and supported by encoder */
                out_stream->time_base = in_stream->time_base;
                out_stream->avg_frame_rate = in_stream->avg_frame_rate;
                enc_ctx->sample_rate = dec_ctx->sample_rate;
                out_stream->r_frame_rate = in_stream->r_frame_rate;
                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
                enc_ctx->pix_fmt = dec_ctx->pix_fmt;
                enc_ctx->has_b_frames = dec_ctx->has_b_frames;
                enc_ctx->qmin = 30;
                enc_ctx->qmax = 100;
                enc_ctx->frame_number = dec_ctx->frame_number;
                enc_ctx->skip_frame = AVDISCARD_NONE;
                enc_ctx->bit_rate = 1000 * 1000;
                enc_ctx->gop_size = 25;
                enc_ctx->keyint_min = 50;
                enc_ctx->max_b_frames = 1000000;
                enc_ctx->b_frame_strategy = 1;
                enc_ctx->coder_type = 1;
                enc_ctx->me_cmp = 1;
                enc_ctx->me_range = 16;
                enc_ctx->me_method = ME_HEX;
                enc_ctx->me_subpel_quality = 4;
                enc_ctx->i_quant_factor = 0;
                enc_ctx->qcompress = 0;
                enc_ctx->max_qdiff = 4;
                enc_ctx->thread_count = 0;
                av_dict_set(&option, "x264opts", "bframes=16:keyint=50:min-keyint=50:no-scenecut", 0);
                av_dict_set(&option, "preset", "slow", 0);
                av_dict_set(&option, "r", "25", 0);
                av_dict_set(&option, "vprofile", "high", 0);
                av_dict_set(&option, "tune", "zerolatency", 0);
            } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO && has_audio == 0) {
                encoder = avcodec_find_encoder(AV_CODEC_ID_AAC);
                if (!encoder) {
                    av_log(NULL, AV_LOG_FATAL, "Necessary audio encoder not found\n");
                    return AVERROR_INVALIDDATA;
                }
                has_audio = 1;
                av_dict_set(&option, "bsf", "aac_adtstoasc", 0);
                av_dict_set(&option, "bsf", "a aac_adtstoasc", 0);
                av_log(NULL, AV_LOG_DEBUG, "Audio stream: %u\n", i);
                enc_ctx->sample_rate = dec_ctx->sample_rate;
                enc_ctx->channel_layout = dec_ctx->channel_layout;
                enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
                /* take first format from list of supported formats */
                enc_ctx->sample_fmt = encoder->sample_fmts[0];
                out_stream->time_base = (AVRational){1, enc_ctx->sample_rate};
            }

            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, &option);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
        } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
            av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } else {
            if (dec_ctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
                av_log(NULL, AV_LOG_ERROR, "subtitle stream: %u\n", i);
                /* if this stream must be remuxed */
                ret = avcodec_copy_context(ofmt_ctx->streams[i]->codec,
                        ifmt_ctx->streams[i]->codec);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Copying stream context failed\n");
                    return ret;
                }
            }
        }

        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

    }
    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    av_log(NULL, AV_LOG_DEBUG, "Successfully prepare output\n");
    if (option != NULL) av_dict_free(&option);
    return 0;
}

static int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
        AVCodecContext *enc_ctx, const char *filter_spec) {
    char args[512];
    int ret = 0;
    AVFilter *buffersrc = NULL;
    AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        snprintf(args, sizeof (args),
                "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                dec_ctx->time_base.num, dec_ctx->time_base.den,
                dec_ctx->sample_aspect_ratio.num,
                dec_ctx->sample_aspect_ratio.den);

        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }

        ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                (uint8_t*) & enc_ctx->pix_fmt, sizeof (enc_ctx->pix_fmt),
                AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    }

    /* Endpoints for the filter graph. */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
            &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int init_filters(int width, int height, AVFormatContext *ifmt_ctx,
        AVFormatContext *ofmt_ctx, FilteringContext *filter_ctx) {
    unsigned int i;
    char filter_spec[512];
    int ret;
    if (!filter_ctx)
        return AVERROR(ENOMEM);
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph = NULL;
        if (!(ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
                || ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;


        if (ifmt_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            av_log(NULL, AV_LOG_DEBUG, "filtering video stream %u\n", i);
            snprintf(filter_spec, sizeof (filter_spec),
                    "scale=iw*min(%d/iw\\,%d/ih):ih*min(%d/iw\\,%d/ih)"
                    ", pad=%d:%d:(%d-iw*min(%d/iw\\,"
                    "%d/ih))/2:(%d-ih*min(%d/iw\\,%d/ih))/2",
                    width, height, width, height, width, height, width, width, height,
                    height, width, height);
        } else {
            av_log(NULL, AV_LOG_ERROR, "ignore stream %u\n", i);
            continue;
        }
        ret = init_filter(&filter_ctx[i], ifmt_ctx->streams[i]->codec,
                ofmt_ctx->streams[i]->codec, filter_spec);
        if (ret)
            return ret;
    }
    return 0;
}

static int encode_write_frame(AVFrame *filt_frame, unsigned int stream_index, int *got_frame,
        AVFormatContext *ofmt_ctx, AVFormatContext *ifmt_ctx) {
    int ret;
    int got_frame_local;
    AVPacket enc_pkt;
    int (*enc_func)(AVCodecContext *, AVPacket *, const AVFrame *, int *) =
            (ifmt_ctx->streams[stream_index]->codec->codec_type ==
            AVMEDIA_TYPE_VIDEO) ? avcodec_encode_video2 : avcodec_encode_audio2;

    if (!got_frame)
        got_frame = &got_frame_local;

    av_log(NULL, AV_LOG_INFO, "Encoding frame\n");
    /* encode filtered frame */
    enc_pkt.data = NULL;
    enc_pkt.size = 0;
    av_init_packet(&enc_pkt);
    ret = enc_func(ofmt_ctx->streams[stream_index]->codec, &enc_pkt,
            filt_frame, got_frame);
    av_frame_free(&filt_frame);
    if (ret < 0)
        return ret;
    if (!(*got_frame))
        return 0;

    /* prepare packet for muxing */
    enc_pkt.stream_index = stream_index;
    av_packet_rescale_ts(&enc_pkt,
            ofmt_ctx->streams[stream_index]->time_base,
            ofmt_ctx->streams[stream_index]->time_base);

    av_log(NULL, AV_LOG_DEBUG, "Muxing video frame\n");
    /* mux encoded frame */
    ret = av_interleaved_write_frame(ofmt_ctx, &enc_pkt);
    return ret;
}

static int filter_encode_write_frame(AVFrame *frame, unsigned int stream_index,
        FilteringContext *filter_ctx, AVFormatContext *ofmt_ctx, AVFormatContext *ifmt_ctx) {
    int ret;
    AVFrame *filt_frame;

    av_log(NULL, AV_LOG_INFO, "Pushing decoded frame to filters\n");
    /* push the decoded frame into the filtergraph */
    ret = av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx,
            frame, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }

    /* pull filtered frames from the filtergraph */
    while (1) {
        filt_frame = av_frame_alloc();
        if (!filt_frame) {
            ret = AVERROR(ENOMEM);
            break;
        }
        av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx,
                filt_frame);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;
            av_frame_free(&filt_frame);
            break;
        }

        filt_frame->pict_type = AV_PICTURE_TYPE_NONE;
        ret = encode_write_frame(filt_frame, stream_index, NULL, ofmt_ctx, ifmt_ctx);
        if (ret < 0)
            break;
    }

    return ret;
}

static int flush_encoder(unsigned int stream_index, AVFormatContext *ofmt_ctx,
        AVFormatContext *ifmt_ctx) {
    int ret;
    int got_frame;

    if (!(ofmt_ctx->streams[stream_index]->codec->codec->capabilities &
            CODEC_CAP_DELAY))
        return 0;

    while (1) {
        av_log(NULL, AV_LOG_INFO, "Flushing stream #%u encoder\n", stream_index);
        ret = encode_write_frame(NULL, stream_index, &got_frame, ofmt_ctx, ifmt_ctx);
        if (ret < 0)
            break;
        if (!got_frame)
            return 0;
    }
    return ret;
}

int convert(const char *input, const char* output) {
    int ret;
    AVPacket packet = {.data = NULL, .size = 0};
    AVFrame *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    int width = 640;
    int height = 360;
    av_log_set_level(AV_LOG_DEBUG);
    av_register_all();
    avfilter_register_all();

    AVFormatContext *ifmt_ctx;
    ifmt_ctx = avformat_alloc_context();
    AVFormatContext *ofmt_ctx;
    AVOutputFormat* output_format;
    output_format = av_guess_format(NULL, output, NULL);
    printf("Output filename: %s\n", output);

    if (output_format != NULL) {
        printf("Output format: %s\n", output_format->name);
    }
    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, output_format, NULL, output);


    FilteringContext *filter_ctx;
    filter_ctx = av_malloc_array(ifmt_ctx->nb_streams, sizeof (*filter_ctx));

    if ((ret = open_input_file(input, ifmt_ctx)) < 0) {
        avformat_free_context(ifmt_ctx);
        av_freep(filter_ctx);
//        avfilter_free(filter_ctx);
//        avformat_free_context(filter_ctx);
        return ret;
    }
    if (ret == 1) {
        width = 1080;
        height = 720;
    } else if (ret == 2) {
        width = 854;
        height = 480;
    }
    if ((ret = open_output_file(output, width, height, ofmt_ctx, ifmt_ctx) < 0)) {
        avformat_free_context(ifmt_ctx);
        avformat_free_context(ofmt_ctx);
        av_freep(filter_ctx);
        return ret;
    }
    if ((ret = init_filters(width, height, ifmt_ctx, ofmt_ctx, filter_ctx)) < 0) {
        avformat_free_context(ifmt_ctx);
        avformat_free_context(ofmt_ctx);
        av_freep(filter_ctx);
        return ret;
    }

    printf("File will be converted to size w: %d, h: %d \n", width, height);
    int vc = 0;
    /* read all packets */
    while (1) {
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
            break;
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codec->codec_type;
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n",
                stream_index);

        if (type == AVMEDIA_TYPE_VIDEO && filter_ctx[stream_index].filter_graph) {
            av_log(NULL, AV_LOG_DEBUG, "Going to reencode&filter the frame\n");
            frame = av_frame_alloc();
            if (!frame) {
                ret = AVERROR(ENOMEM);
                break;
            }
            av_packet_rescale_ts(&packet,
                    ifmt_ctx->streams[stream_index]->time_base,
                    ifmt_ctx->streams[stream_index]->time_base);
            dec_func = avcodec_decode_video2;

            ret = dec_func(ifmt_ctx->streams[stream_index]->codec, frame,
                    &got_frame, &packet);
            if (ret < 0) {
                av_frame_free(&frame);
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }

            if (got_frame) {
                ++vc;
                av_log(NULL, AV_LOG_DEBUG, "processing filter&encode packet index %u\n", stream_index);
                frame->pts = av_frame_get_best_effort_timestamp(frame);
                ret = filter_encode_write_frame(frame, stream_index, filter_ctx, ofmt_ctx, ifmt_ctx);
                av_frame_free(&frame);
                if (ret < 0)
                    goto end;
            } else {
                av_frame_free(&frame);
            }
        } else if (type == AVMEDIA_TYPE_AUDIO || type == AVMEDIA_TYPE_SUBTITLE) {
            /* remux this frame without reencoding */
            av_log(NULL, AV_LOG_DEBUG, "processing audio packet index %u\n", stream_index);
            av_packet_rescale_ts(&packet,
                    ifmt_ctx->streams[stream_index]->time_base,
                    ofmt_ctx->streams[stream_index]->time_base);
            ret = av_interleaved_write_frame(ofmt_ctx, &packet);
            if (ret < 0)
                goto end;
        }
        av_free_packet(&packet);
    }

    /* flush filters and encoders */
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        /* flush filter */
        if (!filter_ctx[i].filter_graph)
            continue;
        ret = filter_encode_write_frame(NULL, i, filter_ctx, ofmt_ctx, ifmt_ctx);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing filter failed\n");
            goto end;
        }

        /* flush encoder */
        ret = flush_encoder(i, ofmt_ctx, ifmt_ctx);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Flushing encoder failed\n");
            goto end;
        }
    }
    av_log(NULL, AV_LOG_DEBUG, "Number of video encoded: %d\n", vc);
    av_write_trailer(ofmt_ctx);
end:
    av_free_packet(&packet);
    av_frame_free(&frame);
    if (ifmt_ctx) {
        for (i = 0; i < ifmt_ctx->nb_streams; i++) {
            avcodec_close(ifmt_ctx->streams[i]->codec);
            if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && ofmt_ctx->streams[i]->codec)
                avcodec_close(ofmt_ctx->streams[i]->codec);
            if (filter_ctx && filter_ctx[i].filter_graph)
                avfilter_graph_free(&filter_ctx[i].filter_graph);
        }
    }
    if (filter_ctx != NULL) av_free(filter_ctx);
    if (ifmt_ctx != NULL) avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    if (ofmt_ctx != NULL) avformat_free_context(ofmt_ctx);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}


