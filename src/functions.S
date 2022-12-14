%define USE_TRAMPOLINE
%define USE_ATOMIC
%define USE_SIMD
; %define USE_REGWIPE
%define USE_PASSING_BUFFER
%define USE_WRPKRU
%define USE_CHECK_CALLER
%define USE_CHECK_RAX
%include "./user-trampoline-arm64/trampoline.S"

make_trampoline_with_NaCl split, _split, T, A;
make_trampoline_with_NaCl extract, _extract, T, B;
