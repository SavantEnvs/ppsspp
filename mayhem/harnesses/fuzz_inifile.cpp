// Fuzzes the INI parser used for ppsspp.ini, controls, cheat and translation files.
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "Common/Data/Format/IniFile.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::istringstream stream(std::string((const char *)data, size));
	IniFile ini;
	if (!ini.Load(stream)) {
		return -1;
	}
	for (auto &section : ini.Sections()) {
		std::vector<std::string> keys;
		section->GetKeys(&keys);
		for (const std::string &key : keys) {
			std::string s;
			int i = 0;
			uint32_t u32 = 0;
			uint64_t u64 = 0;
			bool b = false;
			float f = 0.0f;
			double d = 0.0;
			std::vector<std::string> list;
			section->Get(key, &s);
			section->Get(key, &i);
			section->Get(key, &u32);
			section->Get(key, &u64);
			section->Get(key, &b);
			section->Get(key, &f);
			section->Get(key, &d);
			section->Get(key, &list);
		}
		for (const ParsedIniLine &line : section->Lines()) {
			(void)line.Key();
			(void)line.Value();
		}
		(void)section->ToMap();
	}
	ini.SortSections();
	return 0;
}
