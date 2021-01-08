#ifndef LOG_H_
#define LOG_H_

typedef enum {
    LEVEL_DEBUG = 1,
    LEVEL_INFO,
    LEVEL_WARN,
    LEVEL_ERROR,
    LEVEL_SYS_ERROR
} log_level;


int sys_error(log_level lv, const char* fmt, ...);
int error(log_level lv, const char* fmt, ...);


#define DEBUG(fmt, ...) error(LEVEL_DEBUG, "[%s:%d] "fmt, basename(__FILE__), __LINE__, ##__VA_ARGS__)
#define INFO(fmt, ...) error(LEVEL_INFO, "[%s:%d] "fmt, basename(__FILE__), __LINE__, ##__VA_ARGS__)
#define WARNING(fmt, ...) error(LEVEL_WARN, "[%s:%d] "fmt, basename(__FILE__), __LINE__, ##__VA_ARGS__)
#define ERROR(fmt, ...) error(LEVEL_ERROR, "[%s:%d] "fmt, basename(__FILE__), __LINE__, ##__VA_ARGS__)
#define SYS_ERROR(...) sys_error(LEVEL_SYS_ERROR, "[%s:%d] ", basename(__FILE__), __LINE__, ##__VA_ARGS__)

#endif
