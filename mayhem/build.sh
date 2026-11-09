#!/usr/bin/env bash
#
# mayhem/build.sh — builds the PPSSPP fuzz harnesses and the known-answer selftest.
#
# PPSSPP's own build (CMake over the whole emulator, git submodules, GPU/UI backends) is not usable
# air-gapped, and the fuzzed parsers sit in self-contained files of Common/, Core/ELF and ext/. So the
# harnesses are compiled straight against that slice of the tree (no CMake); mayhem/support/glue.cpp
# supplies the few emulator-wide symbols the slice names. In-tree libpng17 and zlib are compiled with the
# project (instrumented); libzstd comes from apt (-dev, installed by the Dockerfile) and stays as shipped.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS SRC

cd "$SRC"
B="$SRC/mayhem/build"
rm -rf "$B"
mkdir -p "$B/fuzz" "$B/test"

INCLUDES="-I$SRC -I$SRC/Common -I$SRC/ext -I$SRC/ext/libpng17 -I$SRC/ext/zlib"

CXX_SOURCES=(
  Common/Log/LogManager Common/Log Common/File/Path Common/File/FileUtil Common/File/VFS/VFS
  Common/File/DirListing Common/File/AndroidContentURI Common/Data/Text/Parsers Common/Data/Text/I18n
  Common/Data/Encoding/Utf8 Common/Data/Encoding/Compression Common/Data/Format/IniFile
  Common/Data/Format/JSONReader Common/Data/Format/PNGLoad Common/Data/Format/ZIMLoad
  Common/Math/expression_parser Common/Buffer Common/StringUtils Common/TimeUtil Common/SysError
  Common/MemoryUtil Common/Net/URL Core/ELF/ParamSFO ext/gason/gason
  ext/at3_standalone/atrac3 ext/at3_standalone/atrac3plus ext/at3_standalone/atrac3plusdec
  ext/at3_standalone/atrac3plusdsp ext/at3_standalone/atrac ext/at3_standalone/compat
  ext/at3_standalone/fft ext/at3_standalone/get_bits ext/at3_standalone/mem
  mayhem/support/glue
)
C_SOURCES=(
  ext/libpng17/png ext/libpng17/pngerror ext/libpng17/pngget ext/libpng17/pngmem ext/libpng17/pngpread
  ext/libpng17/pngread ext/libpng17/pngrio ext/libpng17/pngrtran ext/libpng17/pngrutil ext/libpng17/pngset
  ext/libpng17/pngtrans ext/libpng17/pngwio ext/libpng17/pngwrite ext/libpng17/pngwtran ext/libpng17/pngwutil
  ext/zlib/adler32 ext/zlib/compress ext/zlib/crc32 ext/zlib/deflate ext/zlib/inffast ext/zlib/inflate
  ext/zlib/inftrees ext/zlib/trees ext/zlib/uncompr ext/zlib/zutil ext/zlib/infback
)
TARGETS=(fuzz_expression fuzz_inifile fuzz_paramsfo fuzz_png fuzz_zim fuzz_atrac)

compile_one() {
  local kind="$1" src="$2" out="$3" flags="$4"
  local obj="$out/$(echo "$src" | tr / _).o"
  # Benign-on-x86 UBSan checks that fire on every parse and mask the memory-safety bugs behind them:
  # over-wide shifts and unchecked (uint32_t)(float op float) casts in the debugger expression evaluator,
  # unaligned u32 loads of PARAM.SFO fields. The float-cast-overflow reports fire on common inputs and would
  # halt the run before the out-of-bounds reads in the same function (e.g. EXOP_TERTELSE) are reached.
  case "$src" in
    Common/Math/expression_parser) flags="$flags -fno-sanitize=shift,float-cast-overflow" ;;
    Core/ELF/ParamSFO) flags="$flags -fno-sanitize=alignment" ;;
  esac
  if [ "$kind" = cc ]; then
    $CC $flags -c "$SRC/$src.c" -o "$obj"
  else
    $CXX -std=c++17 $flags -c "$SRC/$src.cpp" -o "$obj"
  fi
}
export -f compile_one

# compile_slice <outdir> <flags>: compiles the whole slice in parallel into <outdir>/*.o
compile_slice() {
  local out="$1" flags="$2"
  export OUT_DIR="$out" SLICE_FLAGS="$flags"
  { for s in "${CXX_SOURCES[@]}"; do printf 'cxx %s\n' "$s"; done
    for s in "${C_SOURCES[@]}"; do printf 'cc %s\n' "$s"; done; } |
    xargs -P "$MAYHEM_JOBS" -L1 bash -c 'compile_one "$1" "$2" "$OUT_DIR" "$SLICE_FLAGS"' _
}

# 1) Sanitized + SanCov-instrumented slice (-fsanitize=fuzzer-no-link on everything, so the fuzzed code has edges).
FUZZ_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link $INCLUDES"
compile_slice "$B/fuzz" "$FUZZ_FLAGS"
rm -f "$B/libppsspp-slice.a"
ar rcs "$B/libppsspp-slice.a" "$B"/fuzz/*.o

# 2) Harnesses: libFuzzer binary + non-fuzzer run-once reproducer (-standalone) for each.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"
ZSTD_LIBS="-Wl,-Bstatic -lzstd -Wl,-Bdynamic"
build_target() {
  local t="$1"
  $CXX -std=c++17 $FUZZ_FLAGS -c "$SRC/mayhem/harnesses/$t.cpp" -o "$B/$t.o"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$B/$t.o" "$B/lsan_off.o" "$B/libppsspp-slice.a" $ZSTD_LIBS -o "$SRC/$t"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$B/$t.o" "$B/standalone_main.o" "$B/lsan_off.o" "$B/libppsspp-slice.a" $ZSTD_LIBS -o "$SRC/$t-standalone"
}
for t in "${TARGETS[@]}"; do build_target "$t" & done
wait_all() { local rc=0; for j in $(jobs -p); do wait "$j" || rc=1; done; return $rc; }
wait_all

# 3) The known-answer selftest with the project's NORMAL flags (no sanitizers, no -gdwarf-3), for mayhem/test.sh.
TEST_FLAGS="-O2 $COVERAGE_FLAGS $INCLUDES"
compile_slice "$B/test" "$TEST_FLAGS"
$CXX -std=c++17 $TEST_FLAGS -c "$SRC/mayhem/support/selftest.cpp" -o "$B/selftest.o"
$CXX $COVERAGE_FLAGS "$B/selftest.o" "$B"/test/*.o $ZSTD_LIBS -o "$B/selftest"

for t in "${TARGETS[@]}"; do [ -x "$SRC/$t" ] && [ -x "$SRC/$t-standalone" ] || { echo "missing $t" >&2; exit 1; }; done
[ -x "$B/selftest" ]
echo "build.sh: OK"
