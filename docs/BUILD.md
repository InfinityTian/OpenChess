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
- **Stockfish** — the engine used by Singleplayer. If absent, Singleplayer
  reports "Stockfish not found".

### macOS (Homebrew)

```sh
brew install sdl2 sdl2_ttf sdl2_image sdl2_net sdl2_mixer stockfish
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
make            # produces ./openchess
make test       # runs the test suite
make run        # builds and launches
make clean      # removes binaries and object files
```

The Makefile detects SDL2_net with `pkg-config` and defines `HAVE_SDL_NET`
accordingly; every other dependency must be present.

### Assets

Boards and pieces are committed under `assets/` (`boards/`, `pieces/`,
`themes.txt`). To refresh them from the upstream collection:

```sh
scripts/import_assets.sh
```

### Verifying the build

- `make` should finish without warnings.
- `make test` runs `test_rules`, `test_fen`, `test_ai` (skips without Stockfish),
  `test_net` (skips without SDL2_net), `test_local`, and the GUI smoke test.

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
- **Stockfish** —— 单人模式使用的引擎。缺失时单人模式会提示
  “Stockfish not found”。

### macOS（Homebrew）

```sh
brew install sdl2 sdl2_ttf sdl2_image sdl2_net sdl2_mixer stockfish
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
make            # 生成 ./openchess
make test       # 运行测试
make run        # 构建并运行
make clean      # 清理二进制与目标文件
```

Makefile 通过 `pkg-config` 检测 SDL2_net 并据此定义 `HAVE_SDL_NET`；
其余依赖必须存在。

### 资源

棋盘与棋子已提交在 `assets/`（`boards/`、`pieces/`、`themes.txt`）。
如需从上游重新导入：

```sh
scripts/import_assets.sh
```

### 构建校验

- `make` 应无警告完成。
- `make test` 会运行 `test_rules`、`test_fen`、`test_ai`（无 Stockfish 时跳过）、
  `test_net`（无 SDL2_net 时跳过）、`test_local` 以及 GUI 冒烟测试。
