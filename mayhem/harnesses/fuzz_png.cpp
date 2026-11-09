// Fuzzes the PNG loaders (UI assets, texture replacement packs, save icons) over the bundled libpng 1.7.
#include <cstdint>
#include <cstdlib>

#include "Common/Data/Format/PNGLoad.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	int w = 0, h = 0;
	unsigned char *image = nullptr;
	int ok = pngLoadPtr(data, size, &w, &h, &image);
	free(image);
	image = nullptr;
	int okIndexed = pngLoadIndexed(data, size, &w, &h, &image);
	free(image);
	return ok || okIndexed ? 0 : -1;
}
