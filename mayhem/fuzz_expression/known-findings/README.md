Defects the harness finds in unmodified upstream (reproduce with `/mayhem/fuzz_expression-standalone <file>`):

- `tertelse-heap-overflow.repro`: `parsePostfixExpression` reads `exp[num++]` past the end of the postfix vector
  for `EXOP_TERTELSE` (`Common/Math/expression_parser.cpp`, heap-buffer-overflow read).
- `shift-exponent.repro`: `arg[1]<<arg[0]` with an exponent >= 32 (UBSan `shift`). The `shift` check is
  disabled for this one source file in `build.sh` (benign on x86, fired on most inputs and hid the memory bug above).

These files are kept out of `testsuite/` on purpose: seeds must not crash.
