include config.mk

.SUFFIXES:
.SUFFIXES: .o .c

HDR = \
	libysp/ysp/mempool.h \
	libysp/ysp/momcts.h \
	libysp/ysp/pareto.h \
	libysp/ysp/simulate.h \
	libysp/ysp/xorshift.h \
	libysp/ysp/hashset.h
LHDR = \
	libysp/util.h \
	libysp/wfg.h

LIBYSP = libysp.a
LIBYSPSRC = \
	libysp/momcts.c \
	libysp/simulate.c \
	libysp/pareto.c \
	libysp/sample.c \
	libysp/hashset.c \
	libysp/xxhash.c \
	libysp/wfg.c
LIBYSPOBJ = $(LIBYSPSRC:.c=.o)

BIN = \
	example/cliffworld \
	test/mempool_test
OBJ = $(BIN:=.o) $(LIBYSPOBJ)
SRC = $(BIN:=.c)
LIB = $(LIBYSP)

all: $(BIN)

$(BIN): $(LIB) $(@:=.o)

$(OBJ): $(HDR) $(LHDR) config.mk

$(LIBYSP): $(LIBYSPOBJ)
	$(AR) rc $@ $?
	$(RANLIB) $@

.o:
	$(LD) $(LDFLAGS) -o $@ $< $(LIB)
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

install:
	mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	cp -p $(BIN) "$(DESTDIR)$(PREFIX)/bin"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin"/{$(BIN)}

clean:
	rm -f $(OBJ) $(BIN) $(LIB)

.PHONY:
	clean
