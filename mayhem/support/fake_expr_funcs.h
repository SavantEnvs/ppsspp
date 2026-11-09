// Deterministic stand-in for the CPU/symbol-map callbacks of the debugger expression parser.
// Stateless: every evaluation depends only on the expression string.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <strings.h>

#include "Common/Math/expression_parser.h"

// Same register-name vocabulary as the MIPS debug interface.
class FakeFunctions : public IExpressionFunctions {
public:
	bool parseReference(char *str, uint32_t &referenceIndex) override {
		static const char *const names[] = {
			"zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
			"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
			"pc", "hi", "lo", "f0", "f1", "f2", "f3", "f31",
		};
		for (uint32_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
			if (!strcasecmp(str, names[i])) {
				referenceIndex = i;
				return true;
			}
		}
		return false;
	}

	bool parseSymbol(char *str, uint32_t &symbolValue) override {
		if (strncmp(str, "sym", 3) != 0) {
			return false;
		}
		symbolValue = 0x08800000u + (uint32_t)strtoul(str + 3, nullptr, 10) * 4;
		return true;
	}

	uint32_t getReferenceValue(uint32_t referenceIndex) override {
		return referenceIndex * 0x01010101u + 0x1234u;
	}

	ExpressionType getReferenceType(uint32_t referenceIndex) override {
		return referenceIndex >= 35 && referenceIndex < 39 ? EXPR_TYPE_FLOAT : EXPR_TYPE_UINT;
	}

	bool getMemoryValue(uint32_t address, int size, uint32_t &dest, std::string *error) override {
		if (address < 0x08000000u || address >= 0x0A000000u) {
			if (error) {
				*error = "invalid address";
			}
			return false;
		}
		if (size != 1 && size != 2 && size != 4) {
			if (error) {
				*error = "invalid size";
			}
			return false;
		}
		dest = (address * 2654435761u) >> (32 - size * 8);
		return true;
	}
};

