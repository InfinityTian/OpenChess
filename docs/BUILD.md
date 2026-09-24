# Building OpenChess / 构建 OpenChess

This document covers dependencies and building. / 本文介绍依赖与构建方法。

---

## English

### Dependencies

Required:

- A C11 compiler: `cc`, `clang`, or `gcc`
- `make` and `pkg-config`
- **SDL2**, **SDL2_ttf**, **SDL2_image** development packages

Optional:

- **SDL2_net** — enables Local Multiplayer. If absent, OpenChess builds and runs
  but the Local Multiplayer menu entry is disabled.
- **SDL2_mixer** — enables move sounds. If absent, the build runs silently and
  the Settings → Sound option has no effect.
- **libwebsockets** — enables the online (internet) multiplayer transport. If
  absent, the online menu entries are disabled; LAN Local Multiplayer is
  unaffected.
- **Stockfish** — the engine used by Singleplayer. If absent, Singleplayer
  reports "Stockfish not found".

### macOS (Homebrew)

```sh
brew install sdl2 sdl2_ttf sdl2_image sdl2_net sdl2_mixer libwebsockets stockfish
```

### Debian / Ubuntu

```sh
sudo apt update
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev \
    libsdl2-mixer-dev stockfish
```

### Fedora

```sh
sudo dnf install gcc make pkgconf-pkg-config \
    SDL2-devel SDL2_ttf-devel SDL2_image-devel SDL2_net-devel \
    SDL2_mixer-devel stockfish
```

### Windows (native, MSYS2 / MinGW-w64)

OpenChess has a native Windows backend (engine process handling and file
locations), so no POSIX compatibility layer is required at runtime.

```sh
# 1. Install MSYS2 from https://www.msys2.org and open the "MSYS2 UCRT64" shell.
pacman -Syu
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-SDL2 \
    mingw-w64-ucrt-x86_64-SDL2_ttf \
    mingw-w64-ucrt-x86_64-SDL2_image \
    mingw-w64-ucrt-x86_64-SDL2_net \
    mingw-w64-ucrt-x86_64-SDL2_mixer

# 2. Build from the repository root
make CC=gcc              # -> openchess.exe
```

Run `openchess.exe` from the repository root (so that `assets/` is found), or
copy the executable next to the `assets/` folder. The window/taskbar icon is
loaded from `assets/openchess.png`.

**Engine.** Download the Windows build of Stockfish from
<https://stockfishchess.org/download/> and either place `stockfish.exe` on your
`PATH`, or set `engine = C:\path\to\stockfish.exe` in the config file.

**Settings.** `%APPDATA%\openchess\chess.conf`; exported PGNs go to
`%APPDATA%\openchess\games`.

Building a console binary (useful for log output): add the SDL2 libraries
manually instead of letting `pkg-config` add `-mwindows`:

```sh
make CC=gcc SDL_LIBS="-L/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_net -lSDL2_mixer"
```

#### Serving online games on Windows

The online server is a console program (`server/openchessd.exe`) that does not
need SDL, so it can be built on its own:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-libwebsockets   # also pulls in OpenSSL
make CC=gcc server/openchessd.exe
./server/openchessd.exe                # default port 7681, binds all interfaces
./server/openchessd.exe 9000           # or a chosen port
```

Allow inbound TCP in Windows Defender Firewall (PowerShell **as Administrator**):

```powershell
New-NetFirewallRule -DisplayName "OpenChess server" -Direction Inbound `
  -Protocol TCP -LocalPort 7681 -Action Allow
```

Point clients at it with `online_server = ws://<host>:7681/ws` (same machine:
`ws://127.0.0.1:7681/ws`). To serve beyond your LAN, either port-forward TCP
7681 on your router (CGNAT/ISP blocks are common) or run the server on a Linux
VPS behind nginx/Caddy for `wss://`. If the native build fails on `pthread`,
`clock_gettime` or `usleep`, build and run the server under **WSL2** instead (see
below) — the Linux server is the reference target.

### Windows via WSL2 (Linux build)

On Windows 11 with WSLg the Linux build runs unchanged with a GUI:

```sh
sudo apt update
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev \
    libsdl2-mixer-dev stockfish
make
./openchess
```

Keep the checkout in the Linux home directory (accessing `/mnt/c/...` is slow).

### Build

```sh
make            # produces ./openchess (and ./server/openchessd with libwebsockets)
make test       # runs the test suite
make run        # builds and launches
make clean      # removes binaries and object files
```

The Makefile detects SDL2_net and libwebsockets with `pkg-config` and defines
`HAVE_SDL_NET` / `HAVE_WS` accordingly; the other dependencies must be present.
Both are optional: without them the corresponding menu entries are disabled.

### Online server

When libwebsockets is installed, `make` also builds `server/openchessd`. Run it
and point the clients at it (`online_server` in `chess.conf`, default
`ws://127.0.0.1:7681/ws`):

```sh
./server/openchessd            # listens on port 7681
./server/openchessd 9000       # or a chosen port
OPENCHESS_PORT=9000 ./server/openchessd
```

The server is authoritative: it validates moves, runs the clocks and decides the
result. Finished games are appended as JSON lines to `$OPENCHESS_RESULTS` when
that variable is set. For internet play, run it on a host with a public address
and use `wss://` behind a TLS terminator. The `test_online` test starts this
binary automatically and drives two clients.

Accounts and ratings use **SQLite** (detected via `pkg-config sqlite3`,
`HAVE_ACCOUNTS`); the store defaults to `openchess_accounts.db` and can be set
with `OPENCHESS_ACCOUNTS_DB`. Without SQLite the server still runs, with logins
disabled. Puzzle data and the opening book are optional extras:

```sh
pip install huggingface_hub pyarrow   # for the HF source
scripts/import_puzzles.py --count 20000 --min-rating 800 --max-rating 2400
scripts/import_openings.sh     # opening book for the Book classification
```

On Windows (MSYS2/MinGW), build it with `make CC=gcc server/openchessd.exe` and
open the firewall port — see *Serving online games on Windows* above.

### Assets

Boards and pieces are committed under `assets/` (`boards/`, `pieces/`,
`themes.txt`). To refresh them from the upstream collection:

```sh
scripts/import_assets.sh
```

### Verifying the build

- `make` should finish without warnings.
- `make test` runs `test_rules`, `test_fen`, `test_pgn`, `test_ai` (skips without
  Stockfish), `test_transport`, `test_proto`, `test_net` (skips without
  SDL2_net), `test_local`, and the GUI smoke test.

---

## 简体中文

### 依赖

必需：

- C11 编译器：`cc`、`clang` 或 `gcc`
- `make` 与 `pkg-config`
- **SDL2**、**SDL2_ttf**、**SDL2_image** 开发包

可选：

- **SDL2_net** —— 启用本地多人模式。缺失时程序仍可构建运行，但菜单中的
  本地多人项会被禁用。
- **SDL2_mixer** —— 启用走子音效。缺失时程序静默运行，设置中的 Sound 选项无效。
- **libwebsockets** —— 启用在线（互联网）对战传输。缺失时在线菜单项被禁用，
  不影响局域网本地多人。
- **Stockfish** —— 单人模式使用的引擎。缺失时单人模式会提示
  “Stockfish not found”。

### macOS（Homebrew）

```sh
brew install sdl2 sdl2_ttf sdl2_image sdl2_net sdl2_mixer libwebsockets stockfish
```

### Debian / Ubuntu

```sh
sudo apt update
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev \
    libsdl2-mixer-dev stockfish
```

### Fedora

```sh
sudo dnf install gcc make pkgconf-pkg-config \
    SDL2-devel SDL2_ttf-devel SDL2_image-devel SDL2_net-devel \
    SDL2_mixer-devel stockfish
```

### Windows（原生，MSYS2 / MinGW-w64）

OpenChess 现已内置 Windows 后端（引擎进程与文件路径），无需 POSIX 兼容层即可运行。

```sh
# 1. 从 https://www.msys2.org 安装 MSYS2，并打开 “MSYS2 UCRT64” 终端。
pacman -Syu
pacman -S --needed \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-pkgconf \
    mingw-w64-ucrt-x86_64-SDL2 \
    mingw-w64-ucrt-x86_64-SDL2_ttf \
    mingw-w64-ucrt-x86_64-SDL2_image \
    mingw-w64-ucrt-x86_64-SDL2_net \
    mingw-w64-ucrt-x86_64-SDL2_mixer

# 2. 在仓库根目录构建
make CC=gcc              # -> openchess.exe
```

请在仓库根目录运行 `openchess.exe`（以便找到 `assets/`），或将可执行文件与
`assets/` 放在同一目录。窗口/任务栏图标来自 `assets/openchess.png`。

**引擎**：从 <https://stockfishchess.org/download/> 下载 Windows 版 Stockfish，
将 `stockfish.exe` 放入 `PATH`，或在配置文件中设置
`engine = C:\path\to\stockfish.exe`。

**设置**：`%APPDATA%\openchess\chess.conf`；导出的 PGN 位于
`%APPDATA%\openchess\games`。

如需带控制台输出日志，可手动指定 SDL2 库（避免 `pkg-config` 添加 `-mwindows`）：

```sh
make CC=gcc SDL_LIBS="-L/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_net -lSDL2_mixer"
```

#### 在 Windows 上运行在线服务器

在线服务器是控制台程序（`server/openchessd.exe`），不依赖 SDL，可单独构建：

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-libwebsockets   # 同时安装 OpenSSL
make CC=gcc server/openchessd.exe
./server/openchessd.exe                # 默认端口 7681，监听所有网卡
./server/openchessd.exe 9000           # 或指定端口
```

在 Windows Defender 防火墙中放行入站 TCP（以管理员身份运行 PowerShell）：

```powershell
New-NetFirewallRule -DisplayName "OpenChess server" -Direction Inbound `
  -Protocol TCP -LocalPort 7681 -Action Allow
```

客户端用 `online_server = ws://<主机>:7681/ws` 连接（本机为
`ws://127.0.0.1:7681/ws`）。若需跨公网，可在路由器上把 TCP 7681 端口转发到本机
（CGNAT/运营商封锁较常见），或在 Linux VPS 上运行服务器并用 nginx/Caddy 提供
`wss://`。如果原生构建在 `pthread`、`clock_gettime` 或 `usleep` 上失败，请改用
**WSL2** 构建并运行服务器（见下文）——Linux 服务器是参考目标。

### 通过 WSL2 使用 Windows（Linux 构建）

在启用 WSLg 的 Windows 11 上，Linux 构建可直接带界面运行：

```sh
sudo apt update
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev \
    libsdl2-mixer-dev stockfish
make
./openchess
```

建议将仓库放在 Linux 家目录（访问 `/mnt/c/...` 较慢）。

### 构建

```sh
make            # 生成 ./openchess（有 libwebsockets 时还有 ./server/openchessd）
make test       # 运行测试
make run        # 构建并运行
make clean      # 清理二进制与目标文件
```

Makefile 通过 `pkg-config` 检测 SDL2_net 与 libwebsockets，并据此定义
`HAVE_SDL_NET` / `HAVE_WS`；其余依赖必须存在。两者均可选：缺失时对应菜单项被
禁用。

### 在线服务器

安装 libwebsockets 后，`make` 还会构建 `server/openchessd`。运行它，并让客户端
指向它（`chess.conf` 中的 `online_server`，默认 `ws://127.0.0.1:7681/ws`）：

```sh
./server/openchessd            # 监听 7681 端口
./server/openchessd 9000       # 或指定端口
OPENCHESS_PORT=9000 ./server/openchessd
```

服务器为权威端：负责走子校验、计时与胜负判定。设置 `OPENCHESS_RESULTS` 时，已结束
的对局会以 JSON 行追加写入该文件。公网对战请将其部署到有公网地址的主机，并在 TLS
终结后使用 `wss://`。`test_online` 测试会自动启动该程序并驱动两个客户端。

账户与评分使用 **SQLite**（通过 `pkg-config sqlite3` 检测，宏 `HAVE_ACCOUNTS`）；默认
数据库为 `openchess_accounts.db`，可用 `OPENCHESS_ACCOUNTS_DB` 指定。未安装 SQLite 时
服务器仍可运行，但禁用登录。谜题数据与开局库为可选项：

```sh
pip install huggingface_hub pyarrow   # HF 数据源所需
scripts/import_puzzles.py --count 20000 --min-rating 800 --max-rating 2400
scripts/import_openings.sh     # 用于 Book 分类的开局库
```

在 Windows（MSYS2/MinGW）上，用 `make CC=gcc server/openchessd.exe` 构建并放行
防火墙端口 —— 见上文 *在 Windows 上运行在线服务器*。

### 资源

棋盘与棋子已提交在 `assets/`（`boards/`、`pieces/`、`themes.txt`）。
如需从上游重新导入：

```sh
scripts/import_assets.sh
```

### 构建校验

- `make` 应无警告完成。
- `make test` 会运行 `test_rules`、`test_fen`、`test_pgn`、`test_ai`
  （无 Stockfish 时跳过）、`test_transport`、`test_proto`、`test_net`
  （无 SDL2_net 时跳过）、`test_local` 以及 GUI 冒烟测试。
