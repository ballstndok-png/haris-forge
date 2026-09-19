# Haris Forge — Build System
CC       ?= cc
CSTD     ?= -std=c11
CFLAGS   ?= -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
LDFLAGS  ?= -lm -lpthread

PKGS      = openssl libcurl sqlite3
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS) 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs   $(PKGS) 2>/dev/null)

CORE_SRC   = main.c
STUB_SRC   = android_stub.c
BIN        = haris

ifeq ($(wildcard /data/data/com.termux/files/usr),)
SRC = $(CORE_SRC)
else
SRC = $(CORE_SRC) $(STUB_SRC)
endif

.PHONY: all debug clean test install uninstall

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CSTD) $(CFLAGS) $(PKG_CFLAGS) -o $@ $(SRC) $(LDFLAGS) $(PKG_LIBS)

debug: CFLAGS = -O0 -g -Wall -Wextra -Wno-unused-parameter
debug: clean $(BIN)

clean:
	rm -f $(BIN) *.o build.log

test: $(BIN)
	./$(BIN) test --builtin || true
	@if [ -f examples/hello.hr ]; then ./$(BIN) examples/hello.hr; fi

install: $(BIN)
	install -d $(DESTDIR)/usr/local/bin
	install -m 0755 $(BIN) $(DESTDIR)/usr/local/bin/$(BIN)

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(BIN)
