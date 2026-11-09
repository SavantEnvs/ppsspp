`gason-heap-overflow.repro`: `jsonParse` (`ext/gason/gason.cpp`) reads past the end of the buffer even though
`json::JsonReader` copies the input and NUL-terminates it (heap-buffer-overflow read). The cause is the
`while (isspace(*s)) { ++s; if (!*s) break; }` loop: after hitting the NUL it falls through to `*endptr = s++`
and keeps scanning beyond the terminator, so any text ending in whitespace (even the single byte `\n`) trips it.

There is no `fuzz-json` target: libFuzzer's first input on an empty corpus is `\n`, which stops on this defect, and
trimming trailing whitespace in a harness would be input filtering. The target was dropped and the defect is recorded here.
Reproduce by passing the file's bytes to `json::JsonReader` built with ASan.
