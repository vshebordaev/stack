#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

#define _GNU_SOURCE
#include <errno.h>

#ifndef LOG_EMERG
#define LOG_EMERG       0
#endif
#ifndef LOG_ALERT
#define LOG_ALERT       1
#endif
#ifndef LOG_CRIT
#define LOG_CRIT        2
#endif
#ifndef LOG_ERR
#define LOG_ERR         3
#endif
#ifndef LOG_WARNING
#define LOG_WARNING     4
#endif
#ifndef LOG_NOTICE
#define LOG_NOTICE      5
#endif
#ifndef LOG_INFO
#define LOG_INFO        6
#endif
#ifndef LOG_DEBUG
#define LOG_DEBUG       7
#endif

#ifndef DEFAULT_LOG_LEVEL
#define DEFAULT_LOG_LEVEL LOG_INFO
#endif

extern char *program_invocation_short_name;

#ifdef DEBUG
extern int log_level;
#define dbg( lvl, fmt, ... ) do { if (lvl <= log_level) fprintf( stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__); } while (0)
#else
#define dbg( lvl, fmt, ... ) do { if (lvl == LOG_INFO || lvl <= LOG_ERR) fprintf( stderr, "%s: " fmt "\n", program_invocation_short_name, ##__VA_ARGS__); } while (0)
#endif /* DEBUG */

extern const char *errstr(int errnum);

#endif /* __DEBUG_H__ */
