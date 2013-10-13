CC = clang
CFLAGS = -m32 -std=c99 -Wall -ggdb -O0
LDFLAGS = -m32 -ldl
SOURCES = linktest.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = linktest

TESTLIB_CFLAGS = -m32 -std=c99 -Wall -ggdb -O0 -fPIC
TESTLIB_LDFLAGS = -m32 -shared -Wl,-soname -Wl,libtest.so
TESTLIB_SOURCES = testlib.c
TESTLIB_OBJECTS = $(TESTLIB_SOURCES:.c=.o)
TESTLIB = libtest.so

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) $(MAKEFILE_LIST) $(TESTLIB)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

$(TESTLIB): $(TESTLIB_OBJECTS) $(MAKEFILE_LIST)
	$(CC) $(TESTLIB_LDFLAGS) -o $@ $(TESTLIB_OBJECTS)

$(OBJECTS): $(SOURCES) $(SOURCES:.c=.d) $(MAKEFILE_LIST)
	$(CC) -c $(CFLAGS) $< -o $@

$(TESTLIB_OBJECTS): $(TESTLIB_SOURCES) $(TESTLIB_SOURCES:.c=.d) $(MAKEFILE_LIST)
	$(CC) -c $(TESTLIB_CFLAGS) $< -o $@

%.d: %.c
	@$(SHELL) -ec '$(CC) -M $(CFLAGS) $< \
		| sed '\''s/\($*\)\.o[ :]*/\1.o $@ : /g'\'' > $@; \
		[ -s $@ ] || rm -f $@'

clean:
	rm -rf *.o *.d $(EXECUTABLE) $(TESTLIB)

-include $(SOURCES:.c=.d)
