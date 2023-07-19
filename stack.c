#define _GNU_SOURCE
#include <sys/param.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <dirent.h>
#include <threads.h>
#include <string.h>
#include <unistd.h>

#include <link.h>
#include <dlfcn.h>

#include "compat.h"

#include "list.h"

#include "stack.h"
#include "stack-tmpl.h"

#include "debug.h"

struct registry {
	mtx_t lock;
	struct list_head impls;
};

struct stack_impl {
	const char *name;
	const char *alias;
	size_t size;

	atomic_int refcnt;
	const struct stack_tmpl *tmpl;

	struct list_head list;

	void *handle;
	char *path;
};

static struct registry *registry = NULL;

static inline void registry_lock(struct registry *registry)
{
	mtx_lock(&registry->lock);
}

static inline void registry_unlock(struct registry *registry)
{
	mtx_unlock(&registry->lock);
}

static int generic_push(struct stack *stack, value_t top)
{
	return stack->impl->tmpl->push(stack, top);
}

static value_t generic_pop(struct stack *stack)
{
	return stack->impl->tmpl->pop(stack);
}

static value_t generic_min(struct stack *stack)
{
	return stack->impl->tmpl->min(stack);
}

static int generic_init(struct stack *stack, size_t depth)
{
	return stack->impl->tmpl->_init(stack, depth);
}

static void generic_cleanup(struct stack *stack)
{
	stack->impl->tmpl->_cleanup(stack);
}

static inline struct stack_impl *impl_create(const struct stack_tmpl *tmpl)
{
	struct stack_impl *ret;

	ret = calloc(1, sizeof(struct stack_impl));
	if (!ret) {
		dbg(LOG_ERR, "couldn't allocate %lu bytes", sizeof(struct stack_impl));
		goto out;
	}

	list_init(&ret->list);
	atomic_init(&ret->refcnt, 1);

	ret->tmpl = tmpl;
	ret->name = tmpl->name;
	ret->alias = tmpl->alias;
	ret->size = sizeof(struct stack) + ALIGN(tmpl->extra);
out:
	return ret;
}

static void impl_destroy(struct stack_impl *impl)
{
	dlclose(impl->handle);
	free(impl->path);
	free(impl);
}

static inline struct stack_impl *get_impl(struct stack_impl *impl)
{
	struct stack_impl *ret;
	int old;

	ret = impl;
	old = atomic_load_explicit(&impl->refcnt, memory_order_relaxed);
	do {
		if (!old) {
			ret = NULL;
			break;
		}
	} while (!atomic_compare_exchange_weak_explicit(&impl->refcnt, &old, old + 1, memory_order_seq_cst, memory_order_relaxed));

	return ret;
}

static inline void put_impl(struct stack_impl *impl)
{
	if (atomic_fetch_sub(&impl->refcnt, 1) == 1)
		impl_destroy(impl);
}

static const struct stack_impl *__find_impl(const char *name)
{
	struct stack_impl *impl;
	bool found;

	found = false;
	list_for_each_entry(impl, &registry->impls, list)
		if (!strcmp(impl->name, name) || (impl->alias && !strcmp(impl->alias, name))) {
			found = true;
			break;
		}
	return found ? get_impl(impl) : NULL;
}

static inline const struct stack_impl *find_impl(const char *name)
{
	const struct stack_impl *ret;

	registry_lock(registry);
	ret = __find_impl(name);
	registry_unlock(registry);

	return ret;
}

struct stack *stack_create(const char *name, size_t nelts)
{
	struct stack *ret;

	const struct stack_impl *impl;
	size_t size;

	impl = find_impl(name);
	if (!impl) {
		struct stack_impl *new;
		void *handle;

		const struct stack_tmpl *tmpl;
		const struct stack_tmpl *(*get_tmpl)();

		struct link_map *link_map;
		char path[PATH_MAX], *cp;

		new = NULL;

		cp = stpncpy(path, PLUGINS, PATH_MAX);
		*cp++ = '/';
		cp = stpcpy(cp, name);
		cp = stpcpy(cp, SO_EXT);

		errno = 0;
		handle = dlopen(path, RTLD_LAZY);
		if (!handle) {
			dbg(LOG_ERR, "%s", dlerror());
			goto err;
		}

		errno = 0;
		get_tmpl = dlsym(handle, REQUEST_TMPL);
		if (!get_tmpl || !(tmpl = get_tmpl())) {
			dbg(LOG_ERR, "couldn't get template from \"%s\"", path);
			goto close;
		}

		if (strcmp(tmpl->name, name) && strcmp(tmpl->alias, name))
			goto close;

		new = impl_create(tmpl);
		if (!new)
			goto close;

		new->handle = handle;

		if (dlinfo(handle, RTLD_DI_LINKMAP, &link_map)) {
			dbg(LOG_ERR, "dlinfo(\"%s\") failed: %s", path, dlerror());
			new->path = strdup(path);
		} else
			new->path = strdup(link_map->l_name);

		registry_lock(registry);
		impl = __find_impl(name);
		if (!impl) {
			list_add(&new->list, &registry->impls);
			impl = new;
		}
		registry_unlock(registry);
	close:
		if (!impl)
			put_impl(new);
	}

	if (!impl) {
		if (!errno)
			dbg(LOG_ERR, "implementation \"%s\" does not exist", name);
		goto err;
	}

	ret = calloc(1, impl->size);
	if (!ret)
		goto err;

	ret->impl = impl;
	ret->push = generic_push;
	ret->pop = generic_pop;
	ret->min = generic_min;
	ret->_cleanup = generic_cleanup;

	if (generic_init(ret, nelts))
		goto err_free;

	return ret;
err_free:
	free(ret);
err:
	return NULL;
}

static void __constructor(PRIO_HIGH) registry_init(void)
{
	int ret;

	registry = calloc(1, sizeof(struct registry));
	if (!registry) {
		dbg(LOG_CRIT, "couldn't allocate registry: %s", errstr(errno));
		goto err;
	}

	ret = mtx_init(&registry->lock, mtx_plain);
	if (ret != thrd_success) {
		dbg(LOG_CRIT, "mtx_init() failed: %s", errstr(errno));
		goto err;
	}
	list_init(&registry->impls);

	return;
err:
	abort();
}

static void __destructor(PRIO_HIGH) registry_cleanup(void)
{
	struct stack_impl *impl, *tmp;

	list_for_each_entry_safe(impl, tmp, &registry->impls, list) {
		list_del(&impl->list);
		put_impl(impl);
	}
	free(registry);
}


