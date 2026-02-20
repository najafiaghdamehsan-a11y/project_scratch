#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum LogLevel {
        LOG_INFO,
        LOG_WARN,
        LOG_ERROR
    } LogLevel;

    typedef struct LogRecord {
        uint64_t cycle;
        uint64_t line;
        const char* cmd;
        const char* op;
        const char* data;
        LogLevel level;
    } LogRecord;

    void log_init_stdout(void);
    void log_write(LogRecord r);

#ifdef __cplusplus
}
#endif