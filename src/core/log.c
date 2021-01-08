#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "core/sge.h"
#include "core/log.h"

#define MAX_LINE_SIZE 1024

static const char* log_level_en[] = {"UNKNOWN", "DEBUG", "INFO", "WARN", "ERROR", "SYS_ERROR"};


static size_t
format_date(char *buf, size_t maxlen) {
    struct tm lt;
    time_t t = time(NULL);

    localtime_r(&t, &lt);
    return strftime(buf, maxlen, "[%Y-%m-%d %H:%M:%d] ", &lt);
}

static size_t
format_level(char* buf, log_level lv, size_t maxlen) {
    const char* s = log_level_en[lv];
    return snprintf(buf, maxlen, "[%s] ", s);
}

static int
write_log(log_level lv, const char* strerr, const char* fmt, va_list ap) {
    char buf[MAX_LINE_SIZE];
    size_t len = 0;

    len = format_date(buf, MAX_LINE_SIZE);
    len += format_level(buf + len, lv, MAX_LINE_SIZE - len);

    if (fmt) {
        len += vsnprintf(buf + len, sizeof(buf), fmt, ap);
    }

    if (strerr) {
        len += snprintf(buf + len, MAX_LINE_SIZE - len, ": %s", strerr);
    }
    buf[len] = '\0';

    fprintf(stderr, "%s\n", buf);
    return SGE_OK;
}

int
sys_error(log_level lv, const char* fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    write_log(lv, strerror(errno), fmt, ap);
    va_end(ap);
    return SGE_OK;
}

int
error(log_level lv, const char* fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    write_log(lv, NULL, fmt, ap);
    va_end(ap);
    return SGE_OK;
}
