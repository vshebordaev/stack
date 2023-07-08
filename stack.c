#include <sys/param.h>

#include <stdatomic.h>
#include <stdbool.h>
#include <dirent.h>
#include <threads.h>
#include <string.h>
#include <unistd.h>

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
		void *handle;

		char path[PATH_MAX], *cp;
		size_t length;

		DIR *dir;
		struct dirent *de;

		if (!getcwd(path, PATH_MAX)) {
			dbg(LOG_ERR, "couldn't get current directory: %s", errstr(errno));
			goto err;
		}

		cp = path + strlen(path);
		*cp++ = '/';
		cp = stpcpy(cp, DEFAULT_SO_PATH);
		*cp++ = '/';
		*cp = '\0';

		dir = opendir(path);
		if (!dir) {
			dbg(LOG_ERR, "opendir(\"%s\") failed: %s", path, errstr(errno));
			goto err;
		}

		registry_lock(registry);

		errno = 0;
		while ((de = readdir(dir))) {
			const struct stack_tmpl *tmpl;
			const struct stack_tmpl *(*get_tmpl)();

			struct stack_impl *new;
			size_t len;

			len = strlen(de->d_name);
			if (len < sizeof(SO_EXT) - 1 || strcmp(de->d_name + len - sizeof(SO_EXT) + 1, SO_EXT))
				goto leave;
			memcpy(cp, de->d_name, len + 1);

			handle = dlopen(path, RTLD_NOLOAD);
			if (handle)
				goto leave;

			handle = dlopen(path, RTLD_LAZY);
			if (!handle) {
				dbg(LOG_ERR, "%s", dlerror());
				goto leave;
			}

			get_tmpl = dlsym(handle, REQUEST_TMPL);
			if (!get_tmpl)
				goto close;

			tmpl = get_tmpl();
			if (!tmpl) {
				dbg(LOG_ERR, "couldn't get template from \"%s\"", path);
				goto close;
			}

			if (strcmp(tmpl->name, name) && strcmp(tmpl->alias, name))
				goto close;

			new = impl_create(tmpl);
			if (!new) {
				closedir(dir);
				dlclose(handle);
				registry_unlock(registry);
				goto err;
			}

			new->handle = handle;
			new->path = strdup(path);
			list_add(&new->list, &registry->impls);

			impl = new;
			break;
		close:
			dlclose(handle);
		leave:
			errno = 0;
		}
		registry_unlock(registry);

		closedir(dir);

		if (!impl) {
			if (!errno)
				dbg(LOG_ERR, "implementation \"%s\" does not exist", name);
			goto err;
		}
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


