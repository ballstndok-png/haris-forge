# Haris Forge

> A scripting language + runtime for secure multiplayer game servers.

**Version:** 10.2.0
**Author:** Ahmad Bakkour

---

## What is Haris?

Haris is a scripting language designed for one job:
running untrusted multiplayer game servers safely.

Instead of stitching together Lua + Nginx + PyTorch + Docker,
Haris gives you everything in one runtime:

- Game Server - reliable UDP, rooms, matchmaking, anti-cheat
- Sandboxed mods - Landlock/seccomp on Linux, Seatbelt on macOS
- HTTP/3 + WebSocket + WebRTC built in
- AI runtime - Tensor autograd, Transformer, CNN, LSTM, DQN
- Multi-tier JIT - native x86-64 code for hot loops
- Standalone binary pack - no external dependencies

All in one language, with one runtime.

---

## Quick Start

### Linux / macOS / Termux

    git clone https://github.com/ballstndok-png/haris-forge
    cd haris-forge
    make
    ./haris examples/hello.hr

### Requirements

- C11 compiler (gcc / clang)
- OpenSSL 1.1+
- libcurl
- sqlite3
- pthreads

### Install dependencies

Debian / Ubuntu:

    sudo apt install build-essential libssl-dev libcurl4-openssl-dev libsqlite3-dev

macOS:

    brew install openssl curl sqlite

Termux (Android):

    pkg install clang make pkg-config openssl libcurl sqlite

### Windows

WSL (recommended):

    wsl --install
    wsl
    sudo apt install build-essential libssl-dev libcurl4-openssl-dev libsqlite3-dev
    git clone https://github.com/ballstndok-png/haris-forge
    cd haris-forge
    make

MSYS2:

    winget install MSYS2.MSYS2
    pacman -S mingw-w64-x86_64-gcc make openssl curl sqlite3
    git clone https://github.com/ballstndok-png/haris-forge
    cd haris-forge
    make

---

## Project Structure

    haris-forge/
    main.c              - build root (one TU via #include)
    android_stub.c      - Termux/Android stub
    split/              - 26 numbered C files
    examples/hello.hr
    Makefile
    LICENSE
    README.md

---

## Features

- VM + Multi-tier JIT
- Sandbox (Linux/macOS/Windows/BSD)
- Reliable UDP
- Rooms + Matchmaking
- Anti-cheat
- WebSocket + HTTP/3
- Tensor + Autograd
- Transformer / CNN / LSTM
- DQN + Prioritized Replay
- Standalone pack

---

## Security

Every mod runs in an OS-level sandbox by default:

- Linux: Landlock + seccomp-BPF + no_new_privs
- macOS: Seatbelt (sandbox_init)
- Windows: AppContainer + Job Object
- FreeBSD: Capsicum
- OpenBSD: pledge + unveil
- WASM: Host sandbox

Raw hardware access (CPU / GPU) is never granted inside a sandbox.

---

## Platform Support

- Linux x86_64 - Full
- Linux ARM64 - Full
- Termux (Android) - Full (limited sandbox)
- macOS Intel/ARM - Full
- Windows (WSL) - Full
- FreeBSD - Full
- OpenBSD - Full

---

## License

MIT - see LICENSE.
