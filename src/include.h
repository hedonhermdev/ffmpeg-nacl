#include <stdlib.h>
#include <inttypes.h>
#include <libavcodec/avcodec.h>

typedef struct chunk_data {
    uint8_t chunk_num;
    AVCodecParameters* codecpar;
    size_t num_pkts;
    AVPacket* pkts;
} chunk_data;

typedef struct split_data {
    uint8_t *video_buffer;
    size_t video_buffer_size;
    int num_chunks;
    chunk_data* chunks;
    AVCodecParameters* codecpar;
} split_data;
