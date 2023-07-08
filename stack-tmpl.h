#ifndef __STACK_TMPL_H__
#define __STACK_TMPL_H__

#include "types.h"

struct stack_tmpl {
	const char *name;
	const char *alias;
	int extra;

	int (*push)(struct stack *stack, value_t top);
	value_t (*pop)(struct stack *stack);
	value_t (*min)(struct stack *stack);

	int (*_init)(struct stack *stack, int depth);
	void (*_cleanup)(struct stack *stack);
};

static inline void *priv(struct stack *stack)
{
	return stack->private;
}

#endif /* __STACK_TMPL_H__ */
