PROGNAME := stack
SRC := $(filter-out $(LIBSRC),$(wildcard *.c))
OBJ := $(SRC:.c=.o)

LIBNAME := libstack.so
LIBSRC := debug.c stack.c
LIBOBJ := $(LIBSRC:.c=.o)

IMPL_PATH ?= stack-impl
IMPL_SECTION ?= .stack_tmpl

ifneq ($(NDEBUG),)
DEBUG := -DNDEBUG
OPTIM := -O3
else
DEBUG := -DDEBUG -ggdb3
OPTIM := -O0
endif

CCLINK ?= $(CC)

CFLAGS += $(DEBUG) -fPIC $(OPTIM)
LDFLAGS += -ldl -lstack

all binary: $(PROGNAME) impl
.PHONY: all impl

ifeq ($(V),)
Q=@ exec >/dev/null 2>&1;
endif

-include /usr/share/dpkg/architecture.mk

prefix ?= usr/local

DESTDIR ?= $(CURDIR)/debian/stack/
BINDIR ?= $(DESTDIR)/$(prefix)/bin/
LIBDIR ?= $(DESTDIR)/$(prefix)/lib/${DEB_HOST_MULTIARCH}/
PLUGINDIR ?= $(prefix)/lib/${DEB_HOST_MULTIARCH}/$(IMPL_PATH)/

$(PROGNAME): $(LIBNAME) $(OBJ)
	$(CCLINK) -o $@ $(OBJ) -Wl,-rpath='$$LIB' -Wl,-rpath='$$ORIGIN/$(IMPL_PATH)' -Wl,-rpath='$$ORIGIN' -L. $(LDFLAGS)

$(LIBNAME): $(LIBOBJ)
	$(LD) -o $@ -shared $^

impl: impl/.links
	[ "$(IMPL_PATH)" = "impl" ] || ln -s impl $(IMPL_PATH) || :

impl/.links: $(patsubst %.c,%.so,$(wildcard impl/*.c))
	$(Q) for s in $^; do \
		eval $$(eval $$(objdump -t -j $(IMPL_SECTION) $$s|sed -n -E '1,/SYMBOL TABLE:/d;/^\s*$$/d;s/([[:xdigit:]]{16}).*([[:xdigit:]]{16}) ([[:alnum:]]+)/echo \3=$$(dd if=$$s bs=1 skip=$$(echo "ibase=16;\1"|bc) count=$$(echo "ibase=16;\2-1"|bc));/p')); \
		[ -n "$$name" ] && [ $$s != "impl/$$name.so" ] && ( ln -s $$(basename $$s) impl/$$name.so; echo "$(PLUGINDIR)/$$(basename $$s)\t$(PLUGINDIR)/$$name.so" >> $@ ); \
		[ -n "$$alias" ] && [ ! -e "impl/$$alias.so" ] && ( ln -s $$(basename $$s) impl/$$alias.so; echo "$(PLUGINDIR)/$$(basename $$s)\t$(PLUGINDIR)/$$alias.so" >> $@ ); \
	done

impl/%.so: impl/%.o $(LIBNAME)
	$(LD) -o $@ -shared $< -L. -lstack

impl/%.o: impl/%.c
	$(CC) $(CFLAGS) -I. -I.. -o $@ -c $<

install: all
	mkdir -p $(BINDIR) $(LIBDIR) $(LIBDIR)/$(IMPL_PATH)
	strip -s $(PROGNAME)
	install $(PROGNAME) $(BINDIR)
	strip -s $(LIBNAME)
	install $(LIBNAME) $(LIBDIR)
	find impl -type f -name '*.so'|xargs -I% bash -c 'strip -s %; install % $(LIBDIR)/$(IMPL_PATH)'

clean:
	$(RM) $(PROGNAME) $(OBJ) $(LIBNAME) $(LIBOBJ) \
		impl/*.o impl/*.so impl/.links \
		GTAGS GPATH GRTAGS ; \
	$(RM) -r debian/stack; \
	[ -L $(IMPL_PATH) ] &&  $(RM) $(IMPL_PATH) || :

