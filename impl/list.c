#include <errno.h>
#include <threads.h>
#include <stdlib.h>

#include "types.h"

#include "compat.h"

#include "list.h"
#include "stack-tmpl.h"
#include "debug.h"

struct entry {
	struct list_head list;

	value_t data;
};

struct min_entry {
	struct list_head list;

	struct entry *entry;
};

struct list_impl {
	mtx_t lock;

	struct list_head all;
	struct list_head min;
};

static inline void impl_lock(struct list_impl *impl)
{
	mtx_lock(&impl->lock);
}

static inline void impl_unlock(struct list_impl *impl)
{
	mtx_unlock(&impl->lock);
}

static inline value_t __min_value(struct list_impl *impl)
{
	value_t ret;
	struct min_entry *min;

	ret = VALUE_MAX;

	min = list_first_entry(&impl->min, struct min_entry, list);
	if (list_entry_is_head(min, &impl->min, list)) {
		errno = ESRCH;
		goto out;
	}
	ret = min->entry->data;
out:
	return ret;
}

static value_t stack_list_min(struct stack *stack)
{
	value_t ret;
	struct list_impl *impl;

	impl = priv(stack);

	impl_lock(impl);
	ret =  __min_value(impl);
	impl_unlock(impl);

	return ret;
}

static int stack_list_push(struct stack *stack, value_t top)
{
	int ret;
	struct list_impl *impl;
	struct entry *entry;

	impl = priv(stack);

	ret = -1;
	entry = calloc(1, sizeof(struct entry));
	if (!entry) {
		errno = ENOMEM;
		goto out;
	}
	list_init(&entry->list);
	entry->data = top;

	impl_lock(impl);
	list_add(&entry->list, &impl->all);
	if (top < __min_value(impl)) {
		struct min_entry *min;

		min = calloc(1, sizeof(struct min_entry));
		if (!min) {
			errno = ENOMEM;
			impl_unlock(impl);
			goto out;
		}
		min->entry = entry;
		list_add(&min->list, &impl->min);
	}
	impl_unlock(impl);
	ret = 0;
out:
	return ret;
}

static value_t stack_list_pop(struct stack *stack)
{
	int ret;
	struct list_impl *impl;
	struct entry *entry;
	struct min_entry *min;

	impl = priv(stack);

	ret = VALUE_MAX;

	impl_lock(impl);
	entry = list_first_entry(&impl->all, struct entry, list);
	if (list_entry_is_head(entry, &impl->all, list)) {
		errno = ESRCH;
		goto out;
	}

	min = list_first_entry(&impl->min, struct min_entry, list);
	if (min->entry == entry) {
		list_del(&min->list);
		free(min);
	}
	list_del(&entry->list);
	ret = entry->data;
	free(entry);
out:
	impl_unlock(impl);
	return ret;
}

static int stack_list_init(struct stack *stack, int depth UNUSED)
{
	struct list_impl *impl;

	impl = priv(stack);

	mtx_init(&impl->lock, mtx_plain);
	list_init(&impl->all);
	list_init(&impl->min);

	return 0;
}

static void __stack_list_cleanup(struct stack *stack)
{
	struct list_impl *impl;
	struct entry *entry, *tmp;
	struct min_entry *min, *temp;

	impl = priv(stack);

	list_for_each_entry_safe(min, temp, &impl->min, list) {
		list_del(&min->list);
		free(min);
	}
	list_for_each_entry_safe(entry, tmp, &impl->all, list) {
		list_del(&entry->list);
		free(entry);
	}
}

static const struct stack_tmpl list_tmpl = {
	.name = "list",
	.alias = "generic",
	.extra = sizeof(struct list_impl),

	.push = stack_list_push,
	.pop = stack_list_pop,
	.min = stack_list_min,

	._init = stack_list_init,
	._cleanup = __stack_list_cleanup
};

const struct stack_tmpl *__get_tmpl(void)
{
	return &list_tmpl;
}

