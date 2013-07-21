# Common
prefix= /usr/local
libdir= $(prefix)/lib
incdir= $(prefix)/include

CC=   clang

CFLAGS+= -std=c99
CFLAGS+= -Wall -Wextra -Werror -Wsign-conversion
CFLAGS+= -Wno-unused-parameter -Wno-unused-function

LDFLAGS=

# Platform specific
platform= $(shell uname -s)

ifeq ($(platform), Linux)
	CFLAGS+= -DTQ_PLATFORM_LINUX
	CFLAGS+= -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE
endif

# Debug
debug=0
ifeq ($(debug), 1)
	CFLAGS+= -g -ggdb
else
	CFLAGS+= -O2
endif

# Coverage
coverage?= 0
ifeq ($(coverage), 1)
	CC= gcc
	CFLAGS+= -fprofile-arcs -ftest-coverage
	LDFLAGS+= --coverage
endif

# Target: libtaskqueue
libtaskqueue_LIB= libtaskqueue.a
libtaskqueue_SRC= $(wildcard src/*.c)
libtaskqueue_INC= src/taskqueue.h
libtaskqueue_OBJ= $(subst .c,.o,$(libtaskqueue_SRC))

$(libtaskqueue_LIB): CFLAGS+=

# Target: examples
examples_SRC= $(wildcard examples/*.c)
examples_OBJ= $(subst .c,.o,$(examples_SRC))
examples_BIN= $(subst .o,,$(examples_OBJ))

$(examples_BIN): CFLAGS+= -Isrc
$(examples_BIN): LDFLAGS+= -L.
$(examples_BIN): LDLIBS+= -ltaskqueue -pthread

# Rules
all: lib examples

lib: $(libtaskqueue_LIB)

examples: $(examples_BIN)

$(libtaskqueue_LIB): $(libtaskqueue_OBJ)
	$(AR) cr $@ $(libtaskqueue_OBJ)

examples/%: examples/%.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(libtaskqueue_LIB) $(wildcard src/*.o)
	$(RM) $(examples_BIN) $(wildcard examples/*.o)
	$(RM) $(wildcard **/*.gc??)
	$(RM) -r coverage

coverage:
	lcov -o /tmp/libtaskqueue.info -c -d .
	genhtml -o coverage -t libtaskqueue /tmp/libtaskqueue.info
	rm /tmp/libtaskqueue.info

install: lib
	mkdir -p $(libdir) $(incdir)
	install -m 644 $(libtaskqueue_LIB) $(libdir)
	install -m 644 $(libtaskqueue_INC) $(incdir)

uninstall:
	$(RM) $(addprefix $(libdir)/,$(libtaskqueue_LIB))
	$(RM) $(addprefix $(incdir)/,$(libtaskqueue_INC))

tags:
	ctags -o .tags -a $(wildcard src/*.[hc])

.PHONY: all lib examples clean coverage install uninstall tags
