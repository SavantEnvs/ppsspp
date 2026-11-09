// Link-time glue for the fuzz harnesses: the handful of emulator-wide symbols that the small slice of
// Common/ and Core/ we build refers to, without pulling in the Config, Reporting and System modules.
#include <cstdarg>
#include <string_view>

#include "Core/Compatibility.h"
#include "Core/CoreParameter.h"
#include "Core/Reporting.h"

CoreParameter g_CoreParameter;

// Verbatim from Core/Compatibility.cpp (the rest of that file needs Config and the OSD).
void Compatibility::Clear() {
	flags_ = {};
	vrCompat_ = {};
	activeList_.clear();
	filesLoaded_.clear();
}

namespace Reporting {
void ReportMessage(const char *message, ...) {}
bool ShouldLogNTimes(const char *identifier, int n) { return false; }
}  // namespace Reporting

// Verbatim from Core/Util/PathUtil.cpp, which itself needs the whole Config.
bool IsSafePathComponent(std::string_view name) {
	if (name.empty() || name.back() == ' ' || name.back() == '.') {
		return false;
	}
	for (unsigned char c : name) {
		if (c < 0x20 || c == 0x7F || c == '/' || c == '\\' || c == ':') {
			return false;
		}
	}
	return true;
}
