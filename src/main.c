#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

#include "include.h"

int (*_split)(split_data *args) __attribute__((section(".ro_trampoline_data")));;
int (*_extract)(chunk_data args)
    __attribute__((section(".ro_trampoline_data")));;

int fsize(FILE *fp) {
  struct stat sb;

  int ret = fstat(fileno(fp), &sb);
  if (ret < 0) {
    perror("fstat");
    return -1;
  }

  return sb.st_size;
}


AVPacket* copy_packet_buffer(AVPacket* src, size_t num_pkts) {
    size_t sz = num_pkts * sizeof(AVPacket);
    AVPacket* dst = malloc(sz);
    memcpy(dst, src, sz);

    for (int i = 0; i < num_pkts; i++) {
        uint8_t* data = malloc(src[i].size + 64);

        dst[i].size = dst[i].size;

        dst[i].buf = av_buffer_create(data, dst->size + 64, av_buffer_default_free, NULL, 0);

        memset(data, 0, src[i].size + 64);
        memcpy(data, src[i].data, src[i].size);

        dst[i].data = dst[i].buf->data;
    }

    return dst;
}

AVCodecParameters* copy_codecpar(AVCodecParameters* src) {
    AVCodecParameters* dst = malloc(sizeof(AVCodecParameters));
    memcpy(dst, src, sizeof(AVCodecParameters));

    dst->extradata = malloc(src->extradata_size);
    memcpy(dst->extradata, src->extradata, dst->extradata_size);

    return dst;
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
    fprintf(stderr, "domain_alloc: failed\n");
    return 1;
  }

  split_args->video_buffer = video_buffer;
  split_args->video_buffer_size = video_buffer_size;
  split_args->num_chunks = 3;

  split_args->chunks = malloc(3 * sizeof(chunk_data));
  for (int i = 0; i < split_args->num_chunks; i++) {
    split_args->chunks[i].chunk_num = i;
    split_args->chunks[i].num_pkts = 500;
    split_args->chunks[i].pkts = malloc(500 * sizeof(AVPacket));
  }

  void *handle = dlopen("split.so", RTLD_NOW);
  if (handle == NULL) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }
  void *handle2 = dlopen("extract.so", RTLD_NOW);
  if (handle2 == NULL) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return 1;
  }

  int (*_split)(split_data *) = dlsym(handle, "split");
  if (_split == NULL) {
    fprintf(stderr, "dlsym: %s\n", dlerror());
    return 1;
  }

  int (*_extract)(chunk_data) = dlsym(handle2, "extract");
  if (_extract == NULL) {
    fprintf(stderr, "dlsym: %s\n", dlerror());
    return 1;
  }

  int extract_ret[3];

  int split_ret = _split(split_args);

  for (int i = 0; i < split_args->num_chunks; i++) {
      chunk_data chunk = split_args->chunks[i];

      //NACL: copy codecpar
      chunk.codecpar = copy_codecpar(split_args->codecpar);

      // NACL: copy buffer
      chunk.pkts = copy_packet_buffer(chunk.pkts, chunk.num_pkts);

      extract_ret[i] = _extract(chunk);
  }

  
  printf("spli_ret: %d\n", split_ret);
  printf("extract_ret[0]: %d\n", extract_ret[0]);
  printf("extract_ret[1]: %d\n", extract_ret[1]);
  printf("extract_ret[2]: %d\n", extract_ret[2]);

  return 0;
}
