CC       ?= cc
CSTD     ?= -std=c11
CFLAGS   ?= -O2 -std=gnu11 -w -Wno-everything
LDFLAGS  ?= -lm -lpthread

UNAME_S  := $(shell uname -s 2>/dev/null)

ifeq ($(UNAME_S),Darwin)
    CFLAGS += -D_DARWIN_C_SOURCE
endif

ifeq ($(OS),Windows_NT)
    CFLAGS += -D_WIN32_WINNT=0x0600
endif

PKGS       = openssl libcurl sqlite3
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS) 2>/dev/null)
PKG_LIBS   := $(shell pkg-config --libs   $(PKGS) 2>/dev/null)

CORE_SRC   = main.c
COMPAT_SRC = openssl_compat.c
STUB_SRC   = android_stub.c
BIN        = haris

ifeq ($(wildcard /data/data/com.termux/files/usr),)
SRC = $(CORE_SRC) $(COMPAT_SRC)
else
SRC = $(CORE_SRC) $(COMPAT_SRC) $(STUB_SRC)
endif

.PHONY: all clean test install

all: $(BIN)

$(BIN): $(SRC)
$(CC) $(CSTD) $(CFLAGS) $(PKG_CFLAGS) -o $@ $(SRC) $(LDFLAGS) $(PKG_LIBS)

clean:
rm -f $(BIN) *.o

test: $(BIN)
./$(BIN) --version

install: $(BIN)
install -d $(DESTDIR)/usr/local/bin
install -m 0755 $(BIN) $(DESTDIR)/usr/local/bin/$(BIN)
