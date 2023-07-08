#ifndef __TYPES_H__
#define __TYPES_H__

#include <stddef.h>
#include <limits.h>

typedef unsigned int value_t;
#define VALUE_MAX UINT_MAX
#define valfmt "%u"

struct stack_tmpl;

struct stack {
	const struct stack_impl *impl;
	size_t size;

	int (*push)(struct stack *stack, value_t top);
	value_t (*pop)(struct stack *stack);
	value_t (*min)(struct stack *stack);

	void (*_cleanup)(struct stack *stack);
	unsigned long private[0];
};


#endif /* __TYPES_H__ */
