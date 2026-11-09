// Leaks are not what this fleet fuzzes for: turn LeakSanitizer off at build time (ASan/UBSan stay on).
extern "C" int __lsan_is_turned_off() { return 1; }
