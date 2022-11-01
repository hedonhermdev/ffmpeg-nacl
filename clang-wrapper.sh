LLVM_BIN=/home/tirth/ffmpeg-nacl/user-trampoline-arm64/llvm_arm_nacl/build/bin
CC=$LLVM_BIN/clang
LD=$LLVM_BIN/ld.lld
CFLAGS=$ARM_CFLAGS
MUSL=/home/tirth/ffmpeg-nacl/musl

hasNo() {
	pat="$1"
	shift 1

	for e in "$@"; do
		if [ "$e" = "${pat}" ]; then
			return 1
		fi
	done
	return 0
}

ARGS=(
    -fno-stack-protector
    -fomit-frame-pointer
    -fPIC
)

ARGS+=(
	-nostdinc
	-isystem $MUSL/obj/include
	-isystem $MUSL/include
	-isystem $MUSL/arch/x86_64
	-isystem $MUSL/arch/generic
)


if \
	hasNo '-c' "$@" && \
	hasNo '-S' "$@" && \
	hasNo '-E' "$@"
then
	if hasNo '-static' "$@"; then
		ARGS+=("-Wl,-dynamic-linker=${MUSL}/lib/libc.so")
	fi

	ARGS+=("-nostdlib")
	ARGS+=("-L${MUSL}/lib")
	ARGS+=("-Wl,-rpath=${MUSL}/lib")
	ARGS+=("-fuse-ld=${LD:-ld.lld}")

	if hasNo '-nostartfiles' "$@" && \
	   hasNo '-nostdlib' "$@" && \
	   hasNo '-nodefaultlibs' "$@" && \
	   hasNo '-shared' "$@"
	then
		ARGS+=("${MUSL}/lib/crt1.o")
	fi

	if hasNo '-nostdlib' "$@" && \
	   hasNo '-nodefaultlibs' "$@"
	then
		if [[ "${CPP:-}" = "yes" ]]; then
			TAIL+=("-lc++")
			TAIL+=("-lunwind")
			TAIL+=("-lm")
		fi
	fi

    TAIL+=("-Wl,--whole-archive" "${MUSL}/lib/libc.a" "-Wl,--no-whole-archive")
fi

TAIL+=(
    "/nix/store/2n64rqlm78a3j5fsqgcqmbm73zg2wjch-gcc-9.5.0/lib/gcc/aarch64-unknown-linux-gnu/9.5.0/libgcc.a"
    "/nix/store/2n64rqlm78a3j5fsqgcqmbm73zg2wjch-gcc-9.5.0/lib/gcc/aarch64-unknown-linux-gnu/9.5.0/libgcc_eh.a"
)

echo "$CC" "${CFLAGS[@]}" "${ARGS[@]}" "${TAIL[@]}"

exec "$CC" "${CFLAGS[@]}" "${ARGS[@]}" $@ "${TAIL[@]}"
