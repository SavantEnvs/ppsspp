- `pngtrans-stack-underflow.repro`: bundled libpng 1.7 (`ext/libpng17/pngtrans.c`, `png_do_byte_ops_down`) writes
  before the start of a stack buffer while building a transform cache for the image (stack-buffer-underflow).

Kept out of `testsuite/`: seeds must not crash. Reproduce with `/mayhem/fuzz_png-standalone <file>`.
