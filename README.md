# ham-tools

A small suite of command-line amateur-radio tools written in C.

| Tool | What it does |
|------|--------------|
| `qrz` | QRZ.com callsign lookup — one-shot or interactive REPL, with a local SQLite cache and a FIFO for feeding lookups from other programs |
| `qte` | Bearing/heading lookup — geocodes an address (OpenStreetMap Nominatim) and prints the grid square, short-path heading, long-path and compass bearing from your QTH |
| `dxsummit` | Live [DX Summit](https://www.dxsummit.fi/) cluster spots — ncurses TUI (or `--dump` for plain text) |
| `dxheat` | Live [DXHeat](https://www.dxheat.com/) cluster spots — ncurses TUI (or `--dump`) |
| `holycluster` | Live [HolyCluster](https://holycluster.com/) spots over a WebSocket — ncurses TUI (or `--dump`) |

All source lives under [`c/`](c/).

---

## Install with Homebrew

```sh
brew tap Guru-RF/ham-tools
brew install ham-tools
```

This builds all five tools and installs them onto your `PATH`. See
[Packaging for Homebrew](#packaging-for-homebrew) below for how the tap is set up.

> If you have no release tagged yet, install the development version straight
> from `master`:
>
> ```sh
> brew install --HEAD Guru-RF/ham-tools/ham-tools
> ```

---

## Build from source

### Dependencies

The tools link against a handful of system libraries:

| Library | Used by | pkg config |
|---------|---------|------------|
| libcurl | all | `-lcurl` |
| libyaml | all (config parsing) | `yaml-0.1` |
| jansson | `qte`, `dxheat`, `holycluster` | `jansson` |
| libxml2 | `qrz` | `xml2-config` |
| sqlite3 | `qrz` (cache) | `-lsqlite3` |
| readline | `qrz` (interactive REPL) | `-lreadline` |
| ncurses | `dxsummit`, `dxheat`, `holycluster` (TUI) | `-lncurses` |
| libwebsockets | `holycluster` | `libwebsockets` |

Build tools: a C11 compiler (`cc`/`clang`/`gcc`), `make`, and `pkg-config`.

#### macOS (Homebrew)

```sh
brew install pkg-config jansson libyaml libwebsockets readline sqlite libxml2 ncurses
```

`curl` and `xml2-config` ship with macOS. On macOS the Makefile automatically
prefers Homebrew's `readline` (the system `libedit` lacks the callback API `qrz`
needs).

#### Debian / Ubuntu

```sh
sudo apt install build-essential pkg-config \
  libcurl4-openssl-dev libjansson-dev libyaml-dev libwebsockets-dev \
  libreadline-dev libsqlite3-dev libxml2-dev libncurses-dev
```

### Compile

```sh
cd c
make            # builds all five binaries in place
```

The binaries are written next to their sources: `qte/qte`, `qrz/qrz`,
`dxsummit/dxsummit`, `dxheat/dxheat`, `holycluster/holycluster`.

### Install

```sh
make install                 # installs into $HOME/bin   (PREFIX defaults to $HOME)
make install PREFIX=/usr/local   # or anywhere you like  ($PREFIX/bin)
```

### Other make targets

```sh
make clean      # remove object files and binaries
```

The Makefile honours the usual overrides, e.g. `make CC=gcc`,
`make CFLAGS="-O3"`, or `make PREFIX=/opt/ham`.

---

## Configuration

All tools read a single YAML file:

```
~/.config/ham-tools/config.yaml
```

The same directory also holds the `qrz` SQLite cache, REPL history, and the
lookup FIFO. A minimal config:

```yaml
verbose: false

# Your station location — required by `qte` for bearing calculations.
qth:
  latitude:  50.95
  longitude:  4.05

# QRZ.com credentials — required by `qrz`.
qrz:
  com:
    username: "YOURCALL"
    password: "your-qrz-password"

# Optional: override the qrz lookup FIFO path
# fifo: /tmp/qrz.fifo
```

---

## Usage

```sh
qrz W1AW                 # one-shot lookup of one or more callsigns
qrz                      # interactive REPL (readline history; also reads the FIFO)
echo "DL1ABC" > ~/.config/ham-tools/qrz.fifo   # feed a lookup into a running qrz

qte "Brussels, Belgium"  # grid + short/long-path bearing from your QTH

dxsummit                 # live cluster TUI   (q to quit)
dxsummit --dump          # one-shot plain-text dump of current spots
dxheat                   # live cluster TUI
dxheat --dump
holycluster              # live WebSocket cluster TUI
holycluster --dump
```

---

## Packaging for Homebrew

`brew install ham-tools` is served from a **tap** — a GitHub repo named
`homebrew-ham-tools` under the same org. Setting it up once:

1. Create a repo `Guru-RF/homebrew-ham-tools`.
2. Copy [`Formula/ham-tools.rb`](Formula/ham-tools.rb) from this repo into the
   tap's `Formula/` directory.
3. (Recommended) tag a release here so the formula can pin a stable tarball:

   ```sh
   git tag v0.1.0 && git push --tags
   ```

   then fill in `url` + `sha256` in the formula. Get the checksum with:

   ```sh
   curl -sL https://github.com/Guru-RF/ham-tools/archive/refs/tags/v0.1.0.tar.gz | shasum -a 256
   ```

Users then get the short-name install:

```sh
brew tap Guru-RF/ham-tools
brew install ham-tools
```

The formula just delegates to the Makefile's `install` target with a
Homebrew-controlled `PREFIX`, so the build stays in one place.

Try a formula edit locally before pushing it:

```sh
brew install --build-from-source ./Formula/ham-tools.rb
brew test ham-tools
brew audit --new ham-tools
```
