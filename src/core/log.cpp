#include "core/log.h"
#include <iostream>

static std::ostream* g_out = nullptr;

void log_init_stdout(void) {
    g_out = &std::cout;
}

static const char* lvl(LogLevel l) {
    switch (l) {
        case LOG_INFO: return "INFO";
        case LOG_WARN: return "WARN";
        case LOG_ERROR: return "ERROR";
    }
    return "INFO";
}

void log_write(LogRecord r) {
    if (!g_out) return;
    (*g_out)
        << "[Cycle:" << r.cycle << "] "
        << "[Line:" << r.line << "] "
        << "[CMD:" << (r.cmd ? r.cmd : "-") << "] "
        << "[" << lvl(r.level) << "] "
        << (r.op ? r.op : "-") << " -> "
        << (r.data ? r.data : "-")
        << "\n";
}