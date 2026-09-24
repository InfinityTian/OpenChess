# OpenChess

A desktop chess program written in C with SDL2: play both sides in **Analysis**,
battle **Stockfish** in **Singleplayer**, or play a friend over the network in
**Local Multiplayer**. It ships with dozens of chess.com-style boards and piece
sets (from
[GiorgioMegrelli/chess.com-boards-and-pieces](https://github.com/GiorgioMegrelli/chess.com-boards-and-pieces)),
FEN import/export, board flipping, move animations, and SAN text entry.

> Documentation is bilingual (English / 简体中文). 中文说明见下方
> [简体中文](#简体中文) 一节，或阅读 [`README.zh-CN.md`](README.zh-CN.md)。

---

## English

### Features

- **Three modes**
  - **Analysis** – control both sides with full rule enforcement; ideal for
    studying, with FEN import/export, undo/restart and board flipping.
  - **Singleplayer** – play against Stockfish (choose your side and difficulty).
  - **Local Multiplayer** – host or join a second running instance over TCP
    (localhost or LAN).
- **Appearance picker** – browse every board and piece set with thumbnails,
  preview animations, and apply live. Choices are remembered in `chess.conf`.
- **Engine selection** – pick a detected UCI engine or a custom path; used by
  Singleplayer and by live Analysis.
- **Live evaluation bar** – in Analysis a Stockfish evaluation bar sits beside
  the board with score/depth readout.
- **Engine lines and controls** – a 0–5 MultiPV slider shows several engine
  lines (score + variation) in the panel; Threads, Hash, time and depth limits
  are configurable on the Engine screen.
- **Board annotations** – right-click for a circle, right-drag for an arrow,
  with Ctrl/Alt colour variants.
- **Resizable / magnifiable UI** – resize the window or drag the grip in the
  board's bottom-right corner; the board, pieces, panel and menus all scale
  together, keeping the grip under the cursor.
- **Export as PGN** – press `Ctrl+S` (or the **PGN** button) to save the game
  with headers and result.
- **FEN** – load a position from a FEN string and copy the current position.
- **On-demand SAN input** – press Enter to reveal a move box, type SAN
  (`e4`, `Nf3`, `O-O-O`, `e8=Q+`), press Enter to play.
- **Board flipping**, move animations (arcade / slide / fade / none), and an
  undo/restart history.

### Requirements

- A C11 compiler (`cc`/`clang`/`gcc`) and `make`, plus `pkg-config`.
- **SDL2**, **SDL2_ttf**, **SDL2_image**.
- **SDL2_net** – optional; needed only for Local Multiplayer.
- **SDL2_mixer** – optional; needed only for move sounds.
- **Stockfish** – optional; needed only for Singleplayer.

See [`docs/BUILD.md`](docs/BUILD.md) for install commands per platform.

### Build and run

```sh
make            # builds ./openchess
make run        # build and launch
make test       # rules, FEN, AI, networking and GUI smoke tests
```

### Install

Terminal install into `~/.local` (Linux and macOS):

```sh
./install.sh                 # or: make install
PREFIX=/usr/local ./install.sh   # custom prefix
./uninstall.sh               # remove (add --purge to delete settings)
```

macOS app bundle and disk image:

```sh
make dmg        # -> dist/OpenChess.app and dist/OpenChess.dmg
```

See [`docs/INSTALL.md`](docs/INSTALL.md).

### How to play

1. Launch OpenChess; the welcome menu lists the modes.
2. Pick **Analysis**, **Singleplayer (vs AI)** or **Local Multiplayer**.
   - Singleplayer: choose your side and difficulty, then **Start**.
   - Multiplayer: one side **Hosts**, the other **Joins** `127.0.0.1:7777`
     (or the host's LAN IP).
3. Move by clicking a piece and its destination, or press **Enter** and type a
   move in SAN.

Full details and controls: [`docs/USAGE.md`](docs/USAGE.md) and
[`docs/MULTIPLAYER.md`](docs/MULTIPLAYER.md). Internet play is described (as a
plan) in [`docs/ONLINE_MULTIPLAYER.md`](docs/ONLINE_MULTIPLAYER.md).

### Controls

| Key / action | Effect |
| --- | --- |
| Mouse drag/click | Select and move a piece |
| **Enter** | Reveal the SAN input box; **Enter** again submits |
| **Esc** | Cancel SAN/FEN entry, clear selection, or go back |
| **Ctrl+B / Ctrl+P / Ctrl+M** | Cycle board / pieces / animation |
| **Styles** button | Open the visual appearance picker |
| **Ctrl+E** | Open the engine selection screen |
| **Ctrl+S** | Export the game as PGN |
| **Ctrl+F** | Flip the board |
| **Ctrl+U** | Undo (disabled in multiplayer) |
| **Ctrl+R** | Restart (disabled in multiplayer) |
| **Ctrl+C** | Copy the current position as FEN |
| **Ctrl+Q** | Quit |
| Board corner drag | Resize the board |

### Configuration

OpenChess reads `chess.conf` (development) or `~/.config/openchess/chess.conf`
(installed). Appearance changes are saved automatically.

```ini
board = icy_sea      # any key from assets/themes.txt
pieces = cases       # any key from assets/themes.txt
animation = arcade   # arcade | slide | fade | none
board_size = 88      # square size in points (44-150)
engine = /opt/homebrew/bin/stockfish   # optional
```

### Project layout

```
src/        C sources (board, move, pgn, fen, ai, net, themes, paths, gui, main)
assets/     boards/, pieces/ and themes.txt (bundled)
tests/      unit and smoke tests
docs/       bilingual documentation
scripts/    asset import + .app/.dmg helpers
```

### License and assets

Source code: MIT (see `LICENSE`). Board and piece images belong to chess.com and
were collected by a third-party repository — see
[`ATTRIBUTIONS.md`](ATTRIBUTIONS.md).

---

## 简体中文

一款使用 C 与 SDL2 编写的桌面国际象棋程序：可在**分析模式**中控制双方，在
**单人模式**中与 Stockfish 对弈，或在**本地多人模式**中通过网络与好友对局。
内置数十套 chess.com 风格的棋盘与棋子（来自
[GiorgioMegrelli/chess.com-boards-and-pieces](https://github.com/GiorgioMegrelli/chess.com-boards-and-pieces)），
支持 FEN 导入/导出、棋盘翻转、走子动画以及 SAN 文本输入。

### 功能

- **三种模式**
  - **分析（Analysis）**：一人控制双方，严格遵循规则；可导入/导出 FEN、悔棋、
    重新开始以及翻转棋盘。
  - **单人（Singleplayer）**：与 Stockfish 对弈（可选择执子方与难度）。
  - **本地多人（Local Multiplayer）**：通过 TCP 连接另一个运行中的实例
    （本机或局域网）。
- **外观选择器**：用缩略图浏览全部棋盘与棋子，预览动画并即时应用；选择会保存到
  `chess.conf`。
- **FEN**：从 FEN 字符串载入局面，并可复制当前局面。
- **按需 SAN 输入**：按回车显示输入框，输入 SAN（`e4`、`Nf3`、`O-O-O`、
  `e8=Q+`），再按回车落子。
- **棋盘翻转**、走子动画（arcade / slide / fade / none）以及悔棋/重开历史。

### 环境依赖

- C11 编译器（`cc`/`clang`/`gcc`）、`make` 与 `pkg-config`。
- **SDL2**、**SDL2_ttf**、**SDL2_image**。
- **SDL2_net** —— 可选，仅本地多人模式需要。
- **Stockfish** —— 可选，仅单人模式需要。

各平台安装命令见 [`docs/BUILD.md`](docs/BUILD.md)。

### 构建与运行

```sh
make            # 生成可执行文件 ./openchess
make run        # 构建并运行
make test       # 规则、FEN、AI、网络与 GUI 冒烟测试
```

### 安装

在 `~/.local` 下进行终端安装（Linux 与 macOS）：

```sh
./install.sh                 # 或：make install
PREFIX=/usr/local ./install.sh   # 自定义前缀
./uninstall.sh               # 卸载（加 --purge 同时删除设置）
```

macOS 应用包与磁盘映像：

```sh
make dmg        # 生成 dist/OpenChess.app 与 dist/OpenChess.dmg
```

详见 [`docs/INSTALL.md`](docs/INSTALL.md)。

### 玩法

1. 启动 OpenChess，欢迎菜单会列出各模式。
2. 选择**分析**、**单人（对战 AI）**或**本地多人**。
   - 单人：选择执子方与难度，然后点击 **Start**。
   - 多人：一方 **Host**，另一方 **Join** 到 `127.0.0.1:7777`
     （或主机的局域网 IP）。
3. 点击棋子再点目标格即可移动；或按**回车**后用 SAN 输入走子。

完整说明与操作见 [`docs/USAGE.md`](docs/USAGE.md) 与
[`docs/MULTIPLAYER.md`](docs/MULTIPLAYER.md)。互联网对战方案（规划）见
[`docs/ONLINE_MULTIPLAYER.md`](docs/ONLINE_MULTIPLAYER.md)。

### 操作

| 按键 / 操作 | 作用 |
| --- | --- |
| 鼠标拖拽/点击 | 选子与走子 |
| **回车** | 显示 SAN 输入框；再按回车提交 |
| **Esc** | 取消 SAN/FEN 输入、清除选择或返回 |
| **Ctrl+B / Ctrl+P / Ctrl+M** | 循环切换棋盘 / 棋子 / 动画 |
| **Styles** 按钮 | 打开可视化外观选择器 |
| **Ctrl+F** | 翻转棋盘 |
| **Ctrl+U** | 悔棋（多人模式禁用） |
| **Ctrl+R** | 重新开始（多人模式禁用） |
| **Ctrl+C** | 复制当前局面为 FEN |
| **Ctrl+Q** | 退出 |

### 配置

OpenChess 会读取 `chess.conf`（开发环境）或
`~/.config/openchess/chess.conf`（安装后）。外观修改会自动保存。

```ini
board = icy_sea      # assets/themes.txt 中的任意键
pieces = cases       # assets/themes.txt 中的任意键
animation = arcade   # arcade | slide | fade | none
board_size = 88      # 格子大小（44-150）
engine = /opt/homebrew/bin/stockfish   # 可选
```

### 目录结构

```
src/        C 源码（board、move、pgn、fen、ai、net、themes、paths、gui、main）
assets/     boards/、pieces/ 与 themes.txt（随仓库提供）
tests/      单元测试与冒烟测试
docs/       中英双语文档
scripts/    资源导入与 .app/.dmg 辅助脚本
```

### 许可证与素材

源代码：MIT（见 `LICENSE`）。棋盘与棋子图片版权归 chess.com 所有，由第三方仓库
收集 —— 详见 [`ATTRIBUTIONS.md`](ATTRIBUTIONS.md)。
