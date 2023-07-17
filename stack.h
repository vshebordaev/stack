#ifndef __STASK_H__
#define __STACK_H__

#include <stdlib.h>

#include "types.h"

#ifndef DEFAULT_IMPL
#define DEFAULT_IMPL "generic"
#endif

#ifndef SO_EXT
#define SO_EXT ".so"
#endif

#ifndef REQUEST_TMPL
#define REQUEST_TMPL "__get_tmpl"
#endif

extern struct stack *stack_create(const char *name, size_t depth);

static inline int stack_push(struct stack *stack, value_t top)
{
	return stack->push(stack, top);
}

static inline value_t stack_pop(struct stack *stack)
{
	return stack->pop(stack);
}

static inline value_t stack_min(struct stack *stack)
{
	return stack->min(stack);
}

static inline struct stack *stack_init(size_t depth)
{
	return stack_create(DEFAULT_IMPL, depth);
}

static inline void stack_destroy(struct stack *stack)
{
	stack->_cleanup(stack);
	free(stack);
}

#endif /* __STACK_H__ */
