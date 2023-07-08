#include <sys/param.h>

#include <paths.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <time.h>

#include "compat.h"

#include "stack.h"
#include "debug.h"

#ifndef NELTS_DEFAULT
#define NELTS_DEFAULT 1000
#endif /* NELTS_DEFAULT */

#ifndef RNDBYTES
#define RNDBYTES 256
#endif /* RNDBYTES */

#ifndef LINE_SIZE
#define LINE_SIZE 256
#endif

extern char *optarg;
extern int optind, opterr, optopt;

#define SYNTAX_ERROR	1
#define INIT_ERROR	2
#define RUNTIME_ERROR	3

#define ARGSTR ":n:"
static void usage(void)
{
	fprintf(stderr, "usage: %s [-n <nelts>]\n"
		"\t-n <nelts> - number of elements to push (default " stringify(NELTS_DEFAULT) ")"
		"\n", program_invocation_short_name);
}

static char *read_line(FILE *file, char *buf, size_t size)
{
	int ret;
	char *cp;
	int c;

	--size;
	cp = buf + size;
	*cp = '\0';

	ret = fseek(file, -1L, SEEK_CUR);
	if (ret == -1)
		goto out;

	c = fgetc(file);
	if (c == EOF) {
		ret = -1;
		goto out;
	}

	while (size) {
		--size;
		--cp;
		*cp = c;

		ret = fseek(file, -2L, SEEK_CUR);
		if (ret == -1) {
			if (ferror(file))
				goto out;
			ret = 0;
			break;
		}

		c = fgetc(file);
		switch (c) {
		case EOF:
			ret = -1;
			goto out;
		case '\n':
			ret = 0;
			goto out;
		default:;
		}
	}
out:
	return ret ? NULL : cp;
}

static int test_proc(struct stack *stack, int nelts)
{
	int ret;
	value_t current, min;

	FILE *tmp;
	char path[PATH_MAX];
	struct random_data data;
	char statebuf[RNDBYTES];
	long int seed;
	int i, fd;

	ret = -1;

	sprintf(path, _PATH_TMP "%s-XXXXXX.log", program_invocation_short_name);

	fd = mkstemps(path, 4);
	if (fd == -1)
		goto out;

	tmp = fdopen(fd, "w+");
	if (!tmp)
		goto out;

	min = VALUE_MAX;

	seed = time(NULL);
	memset(&data, 0, sizeof(data));
	memset(statebuf, 0, sizeof(statebuf));
	initstate_r(seed, statebuf, RNDBYTES, &data);

	for ( i = 0; i < nelts; i++ ) {
		int cnt;

		random_r(&data, &current);
		if (current < min)
			min = current;

		ret = stack_push(stack, current);
		if (ret)
			goto out;

		cnt = fprintf(tmp, "%d\t" valfmt "\t" valfmt "\n", nelts - i - 1, current, min);
		if (cnt <= 0)
			goto out;

		if (min != stack_min(stack))
			dbg(LOG_ERR, "min value from stack is incorrect");
	}

	ret = fseek(tmp, 0, SEEK_END);
	if (ret == -1)
		goto out;

	ret = -1;
	for ( i = 0; i < nelts; i++ ) {
		int c, cnt, idx;
		char *line, buf[LINE_SIZE];

		line = read_line(tmp, buf, sizeof(buf));
		if (!line)
			goto out;

		cnt = sscanf(line, "%d\t" valfmt "\t" valfmt "\n", &idx, &current, &min);
		if (cnt != 3) {
			ret = -1;
			goto out;
		}

		if (min != stack_min(stack))
			dbg(LOG_ERR, "min mismatch");

		if (current != stack_pop(stack))
			dbg(LOG_ERR, "top mismatch");
	}
	ret = 0;
out:
	close(fd);
	unlink(path);

	return ret;
}

int main(int argc, char * const *argv)
{
	int ret;
	struct stack *stack;
	int c, nelts;

	ret = SYNTAX_ERROR;
	nelts = -1;

	while ((c = getopt(argc, argv, ARGSTR)) != -1)
		switch (c) {
		case 'n':
			nelts = atoi(optarg);
			if (nelts <= 0)
				goto out;
			break;
		default:
			usage();
			goto out;
		}
	argc -= optind;
	argv += optind;

	if (nelts == -1)
		nelts = NELTS_DEFAULT;

	ret = INIT_ERROR;
	stack = stack_init(nelts);
	if (!stack)
		goto out;

	ret = test_proc(stack, nelts);
	if (ret)
		ret = RUNTIME_ERROR;

	stack_destroy(stack);
out:
	return ret;
}
