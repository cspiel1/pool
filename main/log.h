#ifndef LOG_H
#define LOG_H
void logw(const char *fmt, ...);
const char *logr(void);
void log_rewind(void);
void log_clear(void);
#endif
