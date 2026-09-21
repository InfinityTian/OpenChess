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
- **Stockfish** — the engine used by Singleplayer. If absent, Singleplayer
  reports "Stockfish not found".

### macOS (Homebrew)

```sh
brew install sdl2 sdl2_ttf sdl2_image sdl2_net stockfish
```

### Debian / Ubuntu

```sh
sudo apt update
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev stockfish
```

### Fedora

```sh
sudo dnf install gcc make pkgconf-pkg-config \
    SDL2-devel SDL2_ttf-devel SDL2_image-devel SDL2_net-devel stockfish
```

### Windows (MSYS2 / MinGW)

```sh
pacman -S mingw-w64-x86_64-gcc make pkgconf \
    mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf \
    mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_net
```

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
- **Stockfish** —— 单人模式使用的引擎。缺失时单人模式会提示
  “Stockfish not found”。

### macOS（Homebrew）

```sh
brew install sdl2 sdl2_ttf sdl2_image sdl2_net stockfish
```

### Debian / Ubuntu

```sh
sudo apt update
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev stockfish
```

### Fedora

```sh
sudo dnf install gcc make pkgconf-pkg-config \
    SDL2-devel SDL2_ttf-devel SDL2_image-devel SDL2_net-devel stockfish
```

### Windows（MSYS2 / MinGW）

```sh
pacman -S mingw-w64-x86_64-gcc make pkgconf \
    mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf \
    mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_net
```

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
