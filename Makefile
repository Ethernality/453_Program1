C      = gcc
AR      = ar
RANLIB  = ranlib
CFLAGS  = -Wall -g -fPIC -std=c99

LIBSO   = libmalloc.so
LIBA    = libmalloc.a
OBJS    = malloc.o

TEST    = test_malloc
TESTSRC = test_malloc.c

all: $(LIBSO) $(LIBA)

$(LIBSO): $(OBJS)
	$(CC) -shared -o $(LIBSO) $(OBJS)

$(LIBA): $(OBJS)
	$(AR) r $(LIBA) $(OBJS)
	$(RANLIB) $(LIBA)

malloc.o: malloc.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(LIBSO) $(TESTSRC)
	$(CC) -Wall -g -std=c99 -o $(TEST) $(TESTSRC) -L. -lmalloc
	@echo 'Run with LD_LIBRARY_PATH=.:$$LD_LIBRARY_PATH ./$(TEST)'

clean:
	rm -f $(OBJS) $(LIBSO) $(LIBA) $(TEST)

.PHONY: all clean
