#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/timestamp.h>

#include "../src/include.h"


int fsize(FILE *fp) {
  struct stat sb;

  int ret = fstat(fileno(fp), &sb);
  if (ret < 0) {
    perror("fstat");
    return -1;
  }

  return sb.st_size;
}

struct buffer_data {
    uint8_t *ptr;
    size_t left;
};

static int read_pkt(void *opaque, uint8_t *buf, int buf_size)
{
    struct buffer_data *bd = (struct buffer_data *)opaque;
    buf_size = FFMIN(buf_size, bd->left);

    if (!buf_size)
        return AVERROR_EOF;

    /* copy internal buffer data to buf */
    memcpy(buf, bd->ptr, buf_size);
    bd->ptr  += buf_size;
    bd->left -= buf_size;

    return buf_size;
}

static int read_buffer(struct buffer_data bd, AVFormatContext* fmt_ctx, uint8_t* avio_ctx_buffer, AVIOContext *avio_ctx)
{
    int ret;

    ret = avformat_open_input(&fmt_ctx, NULL, NULL, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open input: %d\n", AVERROR(ret));
        return -1;
    }

    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return -1;
    }


    return 0;
}

int split(split_data *args) {
    /* fprintf(stderr, "here inside split\n"); */
    int ret = 0;
    AVFormatContext *fmt_ctx = NULL;
    AVIOContext *avio_ctx = NULL;
    AVPacket *pkt;
    AVFrame *frame;
    uint8_t *avio_ctx_buffer;
    size_t avio_ctx_buffer_size = 4096;

    int nb_frames, frames;

    if (args->num_chunks < 1) {
        return -1;
    }

    struct buffer_data bd = { args->video_buffer, args->video_buffer_size };

    fmt_ctx = avformat_alloc_context();
    if (fmt_ctx == NULL) {
        fprintf(stderr, "could not allocate fmt_ctx");
        return -1;
    }

    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        return AVERROR(ENOMEM);
    }
    avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
            0, &bd, &read_pkt, NULL, NULL);
    if (!avio_ctx) {
        return AVERROR(ENOMEM);
    }
    fmt_ctx->pb = avio_ctx;

    ret = read_buffer(bd, fmt_ctx, avio_ctx_buffer, avio_ctx);
    if (ret < 0) {
        fprintf(stderr, "read_buf: failed\n");
        return -1;
    }

    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        goto end;
    }
    int stream_index = ret;

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    AVCodecParameters *codecpar;
    codecpar = fmt_ctx->streams[stream_index]->codecpar;
    args->codecpar = codecpar;

    for (int i = 0; i < args->num_chunks; i++) {
        int num_pkts = 500;
        int pkt_idx = 0;

        AVPacket* pkts;
        posix_memalign((void**) &pkts, 64, num_pkts * sizeof(AVPacket));

        memset(pkts, 0, num_pkts * sizeof(AVPacket));

        while (pkt_idx < num_pkts) {
            ret = av_read_frame(fmt_ctx, pkt);
            if (ret < 0) {
                break;
            }
            if (pkt->stream_index == stream_index) {
                /* copy_packet(&pkts[pkt_idx], pkt); */
                pkts[pkt_idx] = *pkt;
                /* fprintf(stderr, "chunk: %d pos: %lu pts: %lu\n", i, pkt->pos, pkt->pts); */
                pkt_idx += 1;
            }
        }
        args->chunks[i].chunk_num = i;
        args->chunks[i].num_pkts = pkt_idx;
        args->chunks[i].pkts = pkts;
    }

end:
    if (ret < 0) return 1;

    return 0;
}

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

int main(int argc, char **argv) {
  int ret;
  char *filename;
  FILE *fp;
  uint8_t *video_buffer;
  size_t video_buffer_size;

  if (argc < 2) {
    fprintf(stderr, "usage: %s <filename>", argv[0]);
    return 1;
  }

  filename = argv[1];

  fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror("fopen");
    return 1;
  }

  video_buffer_size = fsize(fp);
  video_buffer = mmap(NULL, video_buffer_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE, fileno(fp), 0);

  split_data *split_args = malloc(sizeof(split_data));
  if (!split_args) {
    perror("malloc");
    return 1;
  }

  split_args->video_buffer = video_buffer;
  split_args->video_buffer_size = video_buffer_size;
  split_args->num_chunks = 3;
  split_args->chunks = malloc(sizeof(chunk_data) * 3);

  int split_ret = split(split_args);

  int extract_ret[3];

  for (int i = 0; i < 3; i++) {
    split_args->chunks[i].codecpar = split_args->codecpar;
    extract_ret[i] = extract(split_args->chunks[i]);
    if (ret < 0) {
        break;
    }
  }

  printf("split_ret: %d\n", split_ret);
  printf("extract_ret: %d\n", extract_ret[0]);
  printf("extract_ret: %d\n", extract_ret[1]);
  printf("extract_ret: %d\n", extract_ret[2]);

  return 0;
}
