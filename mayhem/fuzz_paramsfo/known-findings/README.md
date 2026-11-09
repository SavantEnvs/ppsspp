- `writesfo-max-len.sfo`: `ParamSFOData::WriteSFO` sizes its output from every entry's `param_max_len`, which
  `ReadSFO` copies unchecked from the file (up to 4 GiB), so re-saving a ~400 byte parsed PARAM.SFO tries to
  allocate terabytes. The harness does not call `WriteSFO` (that run would stop at the first such input); the
  selftest exercises it. Reproduce by parsing the file with `ReadSFO` and calling `WriteSFO`.

The unaligned `u32` loads in `ReadSFO` trip UBSan `alignment` on nearly every input and are benign on x86, so
that one check is disabled for `Core/ELF/ParamSFO.cpp` in `build.sh`.
Kept out of `testsuite/`: seeds must not crash.
