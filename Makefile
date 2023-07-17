LIBNAME := libstack.so
LIBSRC := debug.c stack.c
LIBOBJ := $(LIBSRC:.c=.o)

PROGNAME := stack
SRC := $(filter-out $(LIBSRC),$(wildcard *.c))
OBJ := $(SRC:.c=.o)

prefix ?= usr
bindir := $(prefix)/bin
libdir := $(prefix)/lib

ifeq ($(DEB_SOURCE),)
root := $(CURDIR)/
plugindir := impl
else
root ?= /
plugindir = $(libdir)/$(DEB_SOURCE)/$(DEB_HOST_MULTIARCH)
endif

DEFS += -DPLUGINS='"$(root)$(plugindir)"'

ifneq ($(DEBUG),)
DEBUG := -DDEBUG -ggdb3
OPTIM := -Og
endif

CCLINK ?= $(CC)

CFLAGS += $(DEFS) $(DEBUG) -fPIC $(OPTIM)

all build: $(PROGNAME) impl/.links
.PHONY: all

ifeq ($(V),)
Q=@ exec >/dev/null 2>&1;
endif

$(PROGNAME): $(LIBNAME) $(OBJ)
	$(CCLINK) -o $@ --enable-linker-build-id $(OBJ) -L. $(LDFLAGS) -dl -lstack

$(LIBNAME): $(LIBOBJ)
	$(LD) -o $@ -shared --build-id $^ -ldl -lc

IMPL_SECTION ?= .stack_tmpl

impl/.links: $(patsubst %.c,%.so,$(wildcard impl/*.c))
	$(Q) for s in $^; do \
		unset name alias; \
		eval $$(eval $$(objdump -t -j $(IMPL_SECTION) $$s|sed -n -E '1,/SYMBOL TABLE:/d;/^\s*$$/d;s/([[:xdigit:]]{16}).*([[:xdigit:]]{16})\s+([[:alnum:]]+)/echo \3=$$(dd if=$$s bs=1 skip=$$(echo "ibase=16;\1"|bc) count=$$(echo "ibase=16;\2-1"|bc));/p')); \
		[ -n "$$name" ] && [ $$s != "impl/$$name.so" ] && ( cd impl; ln -s $$(basename $$s) $$name.so; ) &&  echo "$(plugindir)/$$(basename $$s) $(plugindir)/$$name.so" >> $@ || :; \
		[ -n "$$alias" ] && [ ! -e "impl/$$alias.so" ] && ( cd impl; ln -s $$(basename $$s) $$alias.so; ) && echo "$(plugindir)/$$(basename $$s) $(plugindir)/$$alias.so" >> $@ || :; \
	done

impl/%.so: impl/%.o
	$(CCLINK) -o $@ -shared --enable-linker-build-id $< $(LDFLAGS)

impl/%.o: impl/%.c
	$(CC) $(CFLAGS) -I. -I.. -o $@ -c $<

install: impl/.links .install

.install: 
	echo "$(PROGNAME) $(bindir)" > $@
	echo "$(LIBNAME) $(libdir)/$(DEB_HOST_MULTIARCH)" >> $@
	find impl -type f -name '*.so'|xargs -I% bash -c 'echo impl/$$(basename %) $(plugindir) >> $@'

clean:
	$(RM) $(PROGNAME) $(OBJ) $(LIBNAME) $(LIBOBJ) \
		impl/*.o impl/*.so impl/.links .install

distclean: clean
	$(RM) GTAGS GPATH GRTAGS .gdbinit

