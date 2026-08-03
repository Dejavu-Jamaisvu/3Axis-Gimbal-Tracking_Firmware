#include "log.h"

static uint8_t runtime_log_level = LOG_LEVEL_INFO;

// log [get / number] [set / number]  
void cliLog(uint8_t argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "get") == 0) {
        cliPrintf("Current Log Level: %d\r\n", logGetRuntimeLevel()); // runtime_log_level 반환
    } else if (argc == 3 && strcmp(argv[1], "set") == 0) {
        uint8_t level = atoi(argv[2]);
        if (level > -1 && level <= LOG_LEVEL_VERBOSE) {
            logSetLevel(level);
            cliPrintf("Log level set to %d\r\n", level);
        } else {
            cliPrintf("Invalid log level. Please enter a value between 0 and %d.\r\n", LOG_LEVEL_VERBOSE);
        }
    } else {
        cliPrintf("Usage: log get\r\n");
        cliPrintf("       log set [level(0~%d)]\r\n", LOG_LEVEL_VERBOSE);
        cliPrintf("Log Levels: 0=FATAL, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=VERBOSE\r\n");
    }
}

bool logInit(void) {
    
    runtime_log_level = LOG_LEVEL_INFO;
    return true;
}

void logSetLevel(uint8_t level) {
    runtime_log_level = level;
}


// Public

uint8_t logGetRuntimeLevel(void) {
    return runtime_log_level;
}

void logPrintf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    uartWrite(0, (uint8_t *)buf, len);
}

