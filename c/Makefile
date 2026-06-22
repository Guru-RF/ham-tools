UNAME_S := $(shell uname -s)

# Detect a native-Windows target built under MSYS2 (MINGW64 / UCRT64 / CLANGARM64).
ifneq (,$(findstring MINGW,$(UNAME_S))$(findstring MSYS,$(UNAME_S))$(findstring UCRT,$(UNAME_S))$(findstring CLANG,$(UNAME_S)))
  WINDOWS := 1
endif

CC      ?= cc

ifdef WINDOWS
  # gnu11 (not strict c11) so MinGW exposes POSIX spellings + M_PI;
  # MinGW auto-appends .exe to linked binaries.
  STD := gnu11
  EXE := .exe
else
  STD := c11
  EXE :=
endif

CFLAGS  ?= -O2 -g -Wall -Wextra -Wno-unused-parameter
CFLAGS  += -std=$(STD)
CPPFLAGS += -Icommon

# --- library flags -------------------------------------------------------

CURL_LIBS    := -lcurl
SQLITE_LIBS  := -lsqlite3

PKG := pkg-config

# ncurses: MSYS2 ships the wide-char build as ncursesw.
ifdef WINDOWS
  NCURSES_CFLAGS := $(shell $(PKG) --cflags ncursesw 2>/dev/null)
  NCURSES_LIBS   := $(shell $(PKG) --libs   ncursesw 2>/dev/null)
  ifeq ($(strip $(NCURSES_LIBS)),)
    NCURSES_LIBS := -lncursesw
  endif
else
  NCURSES_LIBS := -lncurses
endif

XML2_CFLAGS := $(shell xml2-config --cflags 2>/dev/null)
XML2_LIBS   := $(shell xml2-config --libs   2>/dev/null)
ifeq ($(strip $(XML2_LIBS)),)
  XML2_LIBS := -lxml2
endif

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

# readline: brew prefix on macOS, pkg-config on MSYS2, plain -lreadline elsewhere.
ifeq ($(UNAME_S),Darwin)
  BREW := $(shell command -v brew 2>/dev/null)
  ifneq ($(BREW),)
    READLINE_PREFIX := $(shell $(BREW) --prefix readline 2>/dev/null)
  endif
endif
ifneq ($(READLINE_PREFIX),)
  READLINE_CFLAGS := -I$(READLINE_PREFIX)/include
  READLINE_LIBS   := -L$(READLINE_PREFIX)/lib -lreadline
else ifdef WINDOWS
  READLINE_CFLAGS := $(shell $(PKG) --cflags readline 2>/dev/null)
  READLINE_LIBS   := $(shell $(PKG) --libs   readline 2>/dev/null)
  ifeq ($(strip $(READLINE_LIBS)),)
    READLINE_LIBS := -lreadline
  endif
else
  READLINE_CFLAGS :=
  READLINE_LIBS   := -lreadline
endif

# --- static linking (Windows / MinGW portable builds): make STATIC=1 -------
# winget installs portable apps by symlinking each .exe into a Links/ dir; the
# Windows loader then searches THAT dir for DLLs, not the real folder where the
# bundled DLLs live, so dynamically-linked binaries die with STATUS_DLL_NOT_FOUND.
# Linking everything statically removes the external DLLs entirely.
ifeq ($(STATIC),1)
  PKGS := $(PKG) --static
  # Tell the headers the libs are static, else they decorate symbols dllimport.
  CPPFLAGS += -DCURL_STATICLIB -DLIBXML_STATIC -DYAML_DECLARE_STATIC
  # Re-resolve each dependency to its full static (Libs.private) chain.
  CURL_LIBS     := $(shell $(PKGS) --libs libcurl)
  XML2_CFLAGS   := $(shell $(PKGS) --cflags libxml-2.0)
  XML2_LIBS     := $(shell $(PKGS) --libs libxml-2.0)
  SQLITE_LIBS   := $(shell $(PKGS) --libs sqlite3)
  NCURSES_LIBS  := $(shell $(PKGS) --libs ncursesw)
  YAML_LIBS     := $(shell $(PKGS) --libs yaml-0.1)
  JANSSON_LIBS  := $(shell $(PKGS) --libs jansson)
  READLINE_LIBS := $(shell $(PKGS) --libs readline)
  # MSYS2 names the static libwebsockets archive libwebsockets_static.a, so the
  # plain -lwebsockets would otherwise resolve to the import lib (the DLL).
  LWS_LIBS      := $(subst -lwebsockets,-lwebsockets_static,$(shell $(PKGS) --libs libwebsockets))
  # Force static archives, and let ld iterate the circular curl/openssl/ssh2
  # chain inside a group so single-pass resolution doesn't drop symbols.
  LDFLAGS     += -static
  GROUP_START := -Wl,--start-group
  GROUP_END   := -Wl,--end-group
endif

PREFIX ?= $(HOME)

# --- objects -------------------------------------------------------------

COMMON_OBJS    := common/paths.o common/locator.o common/http.o \
                  common/config.o common/compat.o
COMMON_TUI_OBJ := common/spottui.o

QTE_OBJS         := qte/qte.o
QRZ_OBJS         := qrz/qrz.o qrz/qrzapi.o qrz/cache.o qrz/fields.o
DXSUMMIT_OBJS    := dxsummit/dxsummit.o
DXHEAT_OBJS      := dxheat/dxheat.o
HOLYCLUSTER_OBJS := holycluster/holycluster.o

ALL_OBJS := $(COMMON_OBJS) $(COMMON_TUI_OBJ) \
            $(QTE_OBJS) $(QRZ_OBJS) $(DXSUMMIT_OBJS) \
            $(DXHEAT_OBJS) $(HOLYCLUSTER_OBJS)

BINS := qte/qte$(EXE) qrz/qrz$(EXE) dxsummit/dxsummit$(EXE) \
        dxheat/dxheat$(EXE) holycluster/holycluster$(EXE)

.PHONY: all clean install

all: $(BINS)

# --- pattern rules -------------------------------------------------------

common/%.o: common/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(YAML_CFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

qte/%.o: qte/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(JANSSON_CFLAGS) -c -o $@ $<

qrz/%.o: qrz/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(XML2_CFLAGS) $(READLINE_CFLAGS) -c -o $@ $<

dxsummit/%.o: dxsummit/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

dxheat/%.o: dxheat/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(JANSSON_CFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

holycluster/%.o: holycluster/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(JANSSON_CFLAGS) $(LWS_CFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

# --- link ---------------------------------------------------------------

qte/qte$(EXE): $(QTE_OBJS) $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(GROUP_START) $(CURL_LIBS) $(JANSSON_LIBS) $(YAML_LIBS) -lm $(GROUP_END)

qrz/qrz$(EXE): $(QRZ_OBJS) $(COMMON_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(GROUP_START) $(CURL_LIBS) $(XML2_LIBS) $(SQLITE_LIBS) $(READLINE_LIBS) $(YAML_LIBS) $(GROUP_END)

dxsummit/dxsummit$(EXE): $(DXSUMMIT_OBJS) $(COMMON_OBJS) $(COMMON_TUI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(GROUP_START) $(CURL_LIBS) $(NCURSES_LIBS) $(YAML_LIBS) -lpthread -lm $(GROUP_END)

dxheat/dxheat$(EXE): $(DXHEAT_OBJS) $(COMMON_OBJS) $(COMMON_TUI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(GROUP_START) $(CURL_LIBS) $(JANSSON_LIBS) $(NCURSES_LIBS) $(YAML_LIBS) -lpthread -lm $(GROUP_END)

holycluster/holycluster$(EXE): $(HOLYCLUSTER_OBJS) $(COMMON_OBJS) $(COMMON_TUI_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(GROUP_START) $(CURL_LIBS) $(LWS_LIBS) $(JANSSON_LIBS) $(NCURSES_LIBS) $(YAML_LIBS) -lpthread -lm $(GROUP_END)

# --- install / clean ----------------------------------------------------

install: all
	mkdir -p $(PREFIX)/bin
	install -m 755 qte/qte$(EXE)                 $(PREFIX)/bin/qte$(EXE)
	install -m 755 qrz/qrz$(EXE)                 $(PREFIX)/bin/qrz$(EXE)
	install -m 755 dxsummit/dxsummit$(EXE)       $(PREFIX)/bin/dxsummit$(EXE)
	install -m 755 dxheat/dxheat$(EXE)           $(PREFIX)/bin/dxheat$(EXE)
	install -m 755 holycluster/holycluster$(EXE) $(PREFIX)/bin/holycluster$(EXE)

clean:
	rm -f $(ALL_OBJS) $(BINS)
