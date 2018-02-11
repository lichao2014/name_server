#ifndef _LOG_H_INCLUDED
#define _LOG_H_INCLUDED

#include <stdio.h>
#include <unistd.h>

#define LOG(level, fmt, ...)                                            \
    do {                                                                \
        printf(#level "|#%d,%s:%d|", getpid(), __FUNCTION__, __LINE__); \
        printf(fmt, ##__VA_ARGS__);                                     \
        fflush(stdout);                                                 \
    } while(0)

#define DEBUG_LOG(fmt, ...) LOG(DEBUG, fmt, ##__VA_ARGS__)
#define WARN_LOG(fmt, ...) LOG(WARN, fmt, ##__VA_ARGS__)
#define INFO_LOG(fmt, ...) LOG(INFO, fmt, ##__VA_ARGS__)
#define ERROR_LOG(fmt, ...) LOG(ERROR, fmt, ##__VA_ARGS__)

#endif //!_LOG_H_INCLUDED