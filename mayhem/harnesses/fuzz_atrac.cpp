// Fuzzes the standalone Atrac3 / Atrac3+ frame decoders (PSP audio in games, .at3 files, videos).
// Byte 0 picks the codec and its stream configuration; the rest is cut into block_align sized frames.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "ext/at3_standalone/at3_decoders.h"

static const int kMaxFrames = 32;

static void DecodeAtrac3(const uint8_t *data, size_t size, int channels, int unit, int jointStereo) {
	int blockAlign = unit * channels;
	// WAV-format extradata: 14 bytes, coding mode at [6..7], frame factor at [10..11].
	uint8_t extra[14] = {1, 0, 0, 0, 0, 0, (uint8_t)jointStereo, 0, (uint8_t)jointStereo, 0, 1, 0, 0, 0};
	ATRAC3Context *ctx = atrac3_alloc(channels, &blockAlign, extra, sizeof(extra));
	if (!ctx)
		return;
	static float left[4096], right[4096];
	float *out[2] = {left, right};
	for (int i = 0; i < kMaxFrames && size >= (size_t)blockAlign; i++) {
		int nb = 0;
		atrac3_decode_frame(ctx, out, &nb, data, blockAlign);
		data += blockAlign;
		size -= blockAlign;
	}
	atrac3_free(ctx);
}

static void DecodeAtrac3Plus(const uint8_t *data, size_t size, int channels, int blockAlign) {
	ATRAC3PContext *ctx = atrac3p_alloc(channels, &blockAlign);
	if (!ctx)
		return;
	static float left[4096], right[4096];
	float *out[2] = {left, right};
	// get_bits.h: the bitstream buffer must be AV_INPUT_BUFFER_PADDING_SIZE (32) bytes longer than the data.
	std::vector<uint8_t> frame(blockAlign + 32);
	for (int i = 0; i < kMaxFrames && size >= (size_t)blockAlign; i++) {
		int nb = 0;
		std::fill(frame.begin(), frame.end(), 0);
		memcpy(frame.data(), data, blockAlign);
		atrac3p_decode_frame(ctx, out, &nb, frame.data(), blockAlign);
		data += blockAlign;
		size -= blockAlign;
	}
	atrac3p_free(ctx);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	if (size < 2)
		return -1;
	uint8_t cfg = data[0];
	data++;
	size--;
	int channels = 1 + (cfg & 1);
	if (cfg & 2) {
		static const int units[3] = {96, 152, 192};
		DecodeAtrac3(data, size, channels, units[(cfg >> 2) % 3], (cfg >> 4) & 1);
	} else {
		static const int aligns[4] = {0x2e8, 0x178, 0x230, 0x118};
		DecodeAtrac3Plus(data, size, channels, aligns[(cfg >> 2) & 3]);
	}
	return 0;
}
