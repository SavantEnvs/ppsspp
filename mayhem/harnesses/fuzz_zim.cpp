// Fuzzes the ZIM texture container (UI atlas, zlib/zstd compressed mip chains).
#include <cstdint>
#include <cstdlib>

#include "Common/Data/Format/ZIMLoad.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	int width[ZIM_MAX_MIP_LEVELS] = {};
	int height[ZIM_MAX_MIP_LEVELS] = {};
	uint8_t *image[ZIM_MAX_MIP_LEVELS] = {};
	int flags = 0;
	int levels = LoadZIMPtr(data, size, width, height, &flags, image);
	if (!levels) {
		return -1;
	}
	free(image[0]);  // the mip levels are slices of one allocation
	return 0;
}
