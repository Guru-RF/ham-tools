# ham-tools

A small suite of command-line amateur-radio tools written in C.

| Tool | What it does |
| ---- | ------------ |
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

This builds all five tools and installs them onto your `PATH`.

> If Homebrew refuses to load the formula from this untrusted third-party
> tap, trust it once and re-run the install:
>
> ```sh
> brew trust Guru-RF/ham-tools
> ```
>
> To build the latest development version straight from `master` instead:
>
> ```sh
> brew install --HEAD Guru-RF/ham-tools/ham-tools
> ```

---

## Install on Windows (winget)

Native Windows builds are available for both **x64** (Intel/AMD) and **ARM64**:

```pwsh
winget install Guru-RF.ham-tools
```

winget picks the right architecture automatically and puts `qrz`, `qte`,
`dxsummit`, `dxheat` and `holycluster` on your `PATH`. Open a **new** terminal
afterwards so the updated `PATH` takes effect, then create your config file
(see [Configuration](#configuration)) before running `qrz` or `qte`.

Upgrade or remove later with:

```pwsh
winget upgrade Guru-RF.ham-tools
winget uninstall Guru-RF.ham-tools
```

> **Windows differences.** The tools are built as native binaries with MSYS2
> MinGW-w64. One-shot lookups, the `qrz` REPL and the cluster TUIs all work the
> same as on macOS/Linux, with two caveats:
>
> - The `qrz` lookup **FIFO** — and the dx\* TUIs' "feed the selected callsign
>   into a running `qrz`" channel — is a POSIX named pipe and is **disabled on
>   Windows**. One-shot lookups and the interactive REPL are unaffected.
> - Config lives in `%APPDATA%\ham-tools\` instead of `~/.config/ham-tools/`.

---

## Build from source

### Dependencies

The tools link against a handful of system libraries:

| Library | Used by | pkg config |
| ------- | ------- | ---------- |
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

#### Windows (MSYS2 / MinGW-w64)

Install [MSYS2](https://www.msys2.org/) and build from the **UCRT64** shell on
x64, or the **CLANGARM64** shell on Windows-on-ARM. Replace the package prefix
below to match (`mingw-w64-ucrt-x86_64-` for UCRT64,
`mingw-w64-clang-aarch64-` for CLANGARM64):

```sh
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make \
  mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-curl \
  mingw-w64-ucrt-x86_64-libyaml mingw-w64-ucrt-x86_64-jansson \
  mingw-w64-ucrt-x86_64-libxml2 mingw-w64-ucrt-x86_64-sqlite3 \
  mingw-w64-ucrt-x86_64-ncurses mingw-w64-ucrt-x86_64-readline \
  mingw-w64-ucrt-x86_64-libwebsockets
```

The Makefile auto-detects MSYS2 (adds `.exe`, links `ncursesw`, builds with
`-std=gnu11`). On CLANGARM64 build with `make -C c CC=clang`.

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

| Platform | Location |
| -------- | -------- |
| Linux / macOS | `~/.config/ham-tools/config.yaml` |
| Windows | `%APPDATA%\ham-tools\config.yaml` |

The same directory also holds the `qrz` SQLite cache and REPL history (and, on
Linux/macOS, the lookup FIFO). A minimal config:

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

### Editing the config on Windows

The config lives at `%APPDATA%\ham-tools\config.yaml` — which expands to
`C:\Users\<you>\AppData\Roaming\ham-tools\config.yaml` (the `AppData` folder is
hidden in Explorer). The easiest way to create and open it is from PowerShell:

```pwsh
$dir = "$env:APPDATA\ham-tools"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
notepad "$dir\config.yaml"
```

Paste the YAML above, set your `qth` and QRZ.com credentials, and save. A few
Windows-specific tips:

- Save as plain **UTF-8**. If Notepad's *Save as type* adds a `.txt` extension,
  set it to **All files** and keep the name `config.yaml`.
- Use **spaces**, not tabs, for indentation (YAML rejects tabs).
- Paths in the config use forward slashes or escaped backslashes; the default
  cache/history locations need no configuration.

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
