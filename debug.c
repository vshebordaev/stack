#define _GNU_SOURCE
#include <string.h>

#include "debug.h"

#ifndef MSG_SIZE_MAX
#define MSG_SIZE_MAX 128
#endif

int log_level = DEFAULT_LOG_LEVEL;

static __thread char msgbuf[MSG_SIZE_MAX];

const char *errstr(int errnum)
{
	return strerror_r(errnum, msgbuf, MSG_SIZE_MAX);
}

