#!/usr/bin/env bash

set euo pipefail
set +x

FFMPEG_LIBS=( FFmpeg/libavformat/libavformat.a FFmpeg/libavcodec/libavcodec.a FFmpeg/libavutil/libavutil.a )
NACL_CC=./clang-wrapper.sh

# CC=./user-trampoline-arm64/llvm_arm_nacl/build/bin/clang

TAIL+=(
    "/nix/store/2n64rqlm78a3j5fsqgcqmbm73zg2wjch-gcc-9.5.0/lib/gcc/aarch64-unknown-linux-gnu/9.5.0/libgcc.a"
    "/nix/store/2n64rqlm78a3j5fsqgcqmbm73zg2wjch-gcc-9.5.0/lib/gcc/aarch64-unknown-linux-gnu/9.5.0/libgcc_eh.a"
)

$NACL_CC  -g -fPIC -static -shared -o extract.so ./src/extract/extract.c ${FFMPEG_LIBS[@]} -Wl,-Bshareable -Wl,-Bstatic -isystem FFmpeg
$NACL_CC  -g -fPIC -static -shared -o split.so ./src/split/split.c ${FFMPEG_LIBS[@]} -Wl,-Bshareable -Wl,-Bstatic -isystem FFmpeg

gcc -g -o ffmpeg_nacl ./src/main.c FFmpeg/libavutil/libavutil.a -ldl -isystem FFmpeg
