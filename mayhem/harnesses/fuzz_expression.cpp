// Fuzzes the debugger expression parser (Common/Math/expression_parser.cpp): infix -> postfix
// conversion and evaluation.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Common/Math/expression_parser.h"
#include "mayhem/support/fake_expr_funcs.h"


extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	std::string expr((const char *)data, size);  // c_str() is NUL-terminated
	FakeFunctions funcs;

	PostfixExpression postfix;
	if (!initPostfixExpression(expr.c_str(), &funcs, postfix)) {
		(void)getExpressionError();
		return -1;
	}
	uint32_t result = 0;
	if (!parsePostfixExpression(postfix, &funcs, result)) {
		(void)getExpressionError();
	}
	return 0;
}
