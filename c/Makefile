UNAME_S := $(shell uname -s)

CC      ?= cc
CFLAGS  ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -std=c11
CPPFLAGS += -Icommon

# --- library flags -------------------------------------------------------

CURL_LIBS    := -lcurl
SQLITE_LIBS  := -lsqlite3
NCURSES_LIBS := -lncurses

XML2_CFLAGS := $(shell xml2-config --cflags 2>/dev/null)
XML2_LIBS   := $(shell xml2-config --libs   2>/dev/null)
ifeq ($(strip $(XML2_LIBS)),)
  XML2_LIBS := -lxml2
endif

PKG := pkg-config

YAML_CFLAGS := $(shell $(PKG) --cflags yaml-0.1 2>/dev/null)
YAML_LIBS   := $(shell $(PKG) --libs   yaml-0.1 2>/dev/null)
ifeq ($(strip $(YAML_LIBS)),)
  YAML_LIBS := -lyaml
endif

JANSSON_CFLAGS := $(shell $(PKG) --cflags jansson 2>/dev/null)
JANSSON_LIBS   := $(shell $(PKG) --libs   jansson 2>/dev/null)
ifeq ($(strip $(JANSSON_LIBS)),)
  JANSSON_LIBS := -ljansson
endif

LWS_CFLAGS := $(shell $(PKG) --cflags libwebsockets 2>/dev/null)
LWS_LIBS   := $(shell $(PKG) --libs   libwebsockets 2>/dev/null)
ifeq ($(strip $(LWS_LIBS)),)
  LWS_LIBS := -lwebsockets
endif

# Prefer homebrew readline on macOS (system libedit is missing rl_callback_*)
ifeq ($(UNAME_S),Darwin)
  BREW := $(shell command -v brew 2>/dev/null)
  ifneq ($(BREW),)
    READLINE_PREFIX := $(shell $(BREW) --prefix readline 2>/dev/null)
  endif
endif
ifneq ($(READLINE_PREFIX),)
  READLINE_CFLAGS := -I$(READLINE_PREFIX)/include
  READLINE_LIBS   := -L$(READLINE_PREFIX)/lib -lreadline
else
  READLINE_CFLAGS :=
  READLINE_LIBS   := -lreadline
endif

PREFIX ?= $(HOME)

# --- objects -------------------------------------------------------------

COMMON_OBJS    := common/paths.o common/locator.o common/http.o common/config.o
COMMON_TUI_OBJ := common/spottui.o

QTE_OBJS         := qte/qte.o
QRZ_OBJS         := qrz/qrz.o qrz/qrzapi.o qrz/cache.o qrz/fields.o
DXSUMMIT_OBJS    := dxsummit/dxsummit.o
DXHEAT_OBJS      := dxheat/dxheat.o
HOLYCLUSTER_OBJS := holycluster/holycluster.o

ALL_OBJS := $(COMMON_OBJS) $(COMMON_TUI_OBJ) \
            $(QTE_OBJS) $(QRZ_OBJS) $(DXSUMMIT_OBJS) \
            $(DXHEAT_OBJS) $(HOLYCLUSTER_OBJS)

BINS := qte/qte qrz/qrz dxsummit/dxsummit dxheat/dxheat holycluster/holycluster

.PHONY: all clean install

all: $(BINS)

# --- pattern rules -------------------------------------------------------

common/%.o: common/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(YAML_CFLAGS) -c -o $@ $<

qte/%.o: qte/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(JANSSON_CFLAGS) -c -o $@ $<

qrz/%.o: qrz/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(XML2_CFLAGS) $(READLINE_CFLAGS) -c -o $@ $<

dxsummit/%.o: dxsummit/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

dxheat/%.o: dxheat/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(JANSSON_CFLAGS) -c -o $@ $<

holycluster/%.o: holycluster/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(JANSSON_CFLAGS) $(LWS_CFLAGS) -c -o $@ $<

# --- link ---------------------------------------------------------------

qte/qte: $(QTE_OBJS) $(COMMON_OBJS)
	$(CC) -o $@ $^ $(CURL_LIBS) $(JANSSON_LIBS) $(YAML_LIBS) -lm

qrz/qrz: $(QRZ_OBJS) $(COMMON_OBJS)
	$(CC) -o $@ $^ $(CURL_LIBS) $(XML2_LIBS) $(SQLITE_LIBS) $(READLINE_LIBS) $(YAML_LIBS)

dxsummit/dxsummit: $(DXSUMMIT_OBJS) $(COMMON_OBJS) $(COMMON_TUI_OBJ)
	$(CC) -o $@ $^ $(CURL_LIBS) $(NCURSES_LIBS) $(YAML_LIBS) -lpthread -lm

dxheat/dxheat: $(DXHEAT_OBJS) $(COMMON_OBJS) $(COMMON_TUI_OBJ)
	$(CC) -o $@ $^ $(CURL_LIBS) $(JANSSON_LIBS) $(NCURSES_LIBS) $(YAML_LIBS) -lpthread -lm

holycluster/holycluster: $(HOLYCLUSTER_OBJS) $(COMMON_OBJS) $(COMMON_TUI_OBJ)
	$(CC) -o $@ $^ $(CURL_LIBS) $(LWS_LIBS) $(JANSSON_LIBS) $(NCURSES_LIBS) $(YAML_LIBS) -lpthread -lm

# --- install / clean ----------------------------------------------------

install: all
	mkdir -p $(PREFIX)/bin
	install -m 755 qte/qte                      $(PREFIX)/bin/qte
	install -m 755 qrz/qrz                      $(PREFIX)/bin/qrz
	install -m 755 dxsummit/dxsummit            $(PREFIX)/bin/dxsummit
	install -m 755 dxheat/dxheat                $(PREFIX)/bin/dxheat
	install -m 755 holycluster/holycluster      $(PREFIX)/bin/holycluster

clean:
	rm -f $(ALL_OBJS) $(BINS)
