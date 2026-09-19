/* Termux build stub: no NDK liblog available.
   Silent no-op because Haris uses it for optional diagnostics only;
   actual program output goes through printf/stdout. */
int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio; (void)tag; (void)fmt;
    return 0;
}
