#include <stdio.h>
#include <stdlib.h>

#include "../include.h"
#include "libavutil/frame.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>

int decode_pkt(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt) {
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending packet for decoding: %d\n", ret);
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        else if (ret > 0) {
            fprintf(stderr, "error during decoding\n");
            return -ret;
        }
        /* fprintf(stderr, */
        /*         "decoded frame: pts=%s coded_picture_number=%d " */
        /*         "display_picture_number=%d width=%d height=%d\n", */
        /*         av_ts2str(frame->pts), frame->coded_picture_number, */
        /*         frame->display_picture_number, frame->width, frame->height); */
    }

    return 0;
}

int extract(chunk_data args) {
    int ret = 0;
    AVCodecContext *dec_ctx = NULL;
    AVFrame *frame = NULL;
    const AVCodec *dec = NULL;

    frame = av_frame_alloc();
    if (frame == NULL) {
        fprintf(stderr, "extract: failed to allocate AVFrame\n");
    }

    dec = avcodec_find_decoder(args.codecpar->codec_id);
    if (!dec) {
        return AVERROR(EINVAL);
    }

    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        return AVERROR(ENOMEM);
    }

    if ((ret = avcodec_parameters_to_context(dec_ctx, args.codecpar)) < 0) {
        return ret;
    }

    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        return ret;
    }

    /* printf("---------BEGIN_CHUNK#%d num_pkts=%lu---------\n", args.chunk_num, args.num_pkts); */
    /* printf("args->pkts: %p\n", args.pkts); */
    for (int i = 0; i < args.num_pkts; i++) {
        AVPacket pkt = args.pkts[i];
        /* fprintf(stderr, "chunk=%d pkt_idx=%d pkt.pts=%s pkt.pos=%lu\n", */
                /* args.chunk_num, i, av_ts2str(pkt.pts), pkt.pos); */
        ret = decode_pkt(dec_ctx, frame, &pkt);
        if (ret < 0) {
          fprintf(stderr, "extract: failed to decode packet\n");
          break;
        }
    }
    /* printf("---------END_CHUNK#%d---------\n", args.chunk_num); */

end:
    return ret;
}
