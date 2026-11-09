// Fuzzes the PARAM.SFO parser (game metadata in ISOs, EBOOT.PBP and save data).
#include <cstdint>
#include <string>
#include <vector>

#include "Core/ELF/ParamSFO.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	ParamSFOData sfo;
	if (!sfo.ReadSFO(data, size)) {
		return -1;
	}
	for (const std::string &key : sfo.GetKeys()) {
		(void)sfo.GetValueString(key);
		(void)sfo.GetValueInt(key);
		unsigned int len = 0;
		(void)sfo.GetValueData(key, &len);
	}
	(void)sfo.GetDiscID();
	(void)sfo.GetSystemVersion();
	(void)sfo.GetDataOffset(data, size, "TITLE");

	return 0;
}
