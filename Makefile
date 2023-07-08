PROGNAME := stack
SRC := $(filter-out $(LIBSRC),$(wildcard *.c))
OBJ := $(SRC:.c=.o)

LIBNAME := libstack.so
LIBSRC := debug.c stack.c
LIBOBJ := $(LIBSRC:.c=.o)

ifneq ($(NDEBUG),)
DEBUG := -DNDEBUG
OPTIM := -O3
else
DEBUG := -DDEBUG -ggdb3
OPTIM := -O0
endif

CCLINK ?= $(CC)

CFLAGS += $(DEBUG) -fPIC $(OPTIM)
LDFLAGS += -ldl -Wl,-rpath=$(shell pwd) -L. -lstack

all: $(PROGNAME) impl
.PHONY: all impl

$(PROGNAME): $(LIBNAME) $(OBJ)
	$(CCLINK) -o $@ $(OBJ) $(LDFLAGS)

$(LIBNAME): $(LIBOBJ)
	$(LD) -o $@ -shared $^

impl: $(patsubst %.c,%.so,$(wildcard impl/*.c))

impl/%.so: impl/%.o $(LIBNAME)
	$(LD) -o $@ -shared $< -L. -lstack

impl/%.o: impl/%.c
	$(CC) $(CFLAGS) -I. -I.. -o $@ -c $<

clean:
	$(RM) $(PROGNAME) $(OBJ) $(LIBNAME) $(LIBOBJ) \
		impl/*.o impl/*.so \
		GTAGS GPATH GRTAGS

