# OpenChess

一款使用 C 与 SDL2 编写的桌面国际象棋程序：可在**分析模式**中控制双方，在
**单人模式**中与 Stockfish 对弈，或在**本地多人模式**中通过网络与好友对局。
内置数十套 chess.com 风格的棋盘与棋子（来自
[GiorgioMegrelli/chess.com-boards-and-pieces](https://github.com/GiorgioMegrelli/chess.com-boards-and-pieces)），
支持 FEN 导入/导出、棋盘翻转、走子动画以及 SAN 文本输入。

> English documentation: [`README.md`](README.md).

## 功能

- **三种模式**
  - **分析（Analysis）**：一人控制双方，严格遵循规则；可导入/导出 FEN、悔棋、
    重新开始以及翻转棋盘。
  - **单人（Singleplayer）**：与 Stockfish 对弈（可选择执子方与难度）。
  - **本地多人（Local Multiplayer）**：通过 TCP 连接另一个运行中的实例
    （本机或局域网）。
- **外观选择器**：用缩略图浏览全部棋盘与棋子，预览动画并即时应用；选择会保存到
  `chess.conf`。
- **引擎选择**：从自动检测到的 UCI 引擎或自定义路径中选择；用于单人模式与分析。
- **实时评估条**：分析模式下棋盘左侧显示 Stockfish 评估条与分数/深度。
- **引擎线路与控制**：面板中的 0–5 MultiPV 滑块可显示多条引擎线路（分数与变着）；
  Engine 界面可设置线程、Hash、时限与深度。
- **棋盘标注**：右键画圆圈，右键拖动画箭头，结合 Ctrl/Alt 可切换颜色。
- **可缩放界面**：缩放系统窗口，或拖动棋盘右下角手柄；棋盘、棋子、面板与菜单会
  一起放大/缩小，并让手柄始终跟随光标。
- **导出 PGN**：按 `Ctrl+S`（或 **Save PGN** 按钮）保存对局（含头部与结果）。
- **FEN**：从 FEN 字符串载入局面，并可复制当前局面。
- **按需 SAN 输入**：按回车显示输入框，输入 SAN（`e4`、`Nf3`、`O-O-O`、
  `e8=Q+`），再按回车落子。
- **棋盘翻转**、走子动画（arcade / slide / fade / none）以及悔棋/重开历史。

## 环境依赖

- C11 编译器（`cc`/`clang`/`gcc`）、`make` 与 `pkg-config`。
- **SDL2**、**SDL2_ttf**、**SDL2_image**。
- **SDL2_net** —— 可选，仅本地多人模式需要。
- **SDL2_mixer** —— 可选，仅走子音效需要。
- **Stockfish** —— 可选，仅单人模式需要。

各平台安装命令见 [`docs/BUILD.md`](docs/BUILD.md)。

### macOS（Homebrew）

```sh
brew install sdl2 sdl2_ttf sdl2_image sdl2_net stockfish
```

### Debian / Ubuntu

```sh
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev stockfish
```

## 构建与运行

```sh
make            # 生成可执行文件 ./openchess
make run        # 构建并运行
make test       # 规则、FEN、AI、网络与 GUI 冒烟测试
```

## 安装

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

## 玩法

1. 启动 OpenChess，欢迎菜单会列出各模式。
2. 选择**分析**、**单人（对战 AI）**或**本地多人**。
   - 单人：选择执子方与难度，然后点击 **Start**。
   - 多人：一方 **Host**，另一方 **Join** 到 `127.0.0.1:7777`
     （或主机的局域网 IP）。
3. 点击棋子再点目标格即可移动；或按**回车**后用 SAN 输入走子。

完整说明与操作见 [`docs/USAGE.md`](docs/USAGE.md) 与
[`docs/MULTIPLAYER.md`](docs/MULTIPLAYER.md)。

## 操作

| 按键 / 操作 | 作用 |
| --- | --- |
| 鼠标拖拽/点击 | 选子与走子 |
| **回车** | 显示 SAN 输入框；再按回车提交 |
| **Esc** | 取消 SAN/FEN 输入、清除选择或返回 |
| **Ctrl+B / Ctrl+P / Ctrl+M** | 循环切换棋盘 / 棋子 / 动画 |
| **Styles** 按钮 | 打开可视化外观选择器 |
| **Ctrl+E** | 打开引擎选择界面 |
| **Ctrl+S** | 导出对局为 PGN |
| **Ctrl+F** | 翻转棋盘 |
| **Ctrl+U** | 悔棋（多人模式禁用） |
| **Ctrl+R** | 重新开始（多人模式禁用） |
| **Ctrl+C** | 复制当前局面为 FEN |
| **Ctrl+Q** | 退出 |
| 拖动棋盘角 | 缩放棋盘 |

## 配置

OpenChess 会读取 `chess.conf`（开发环境）或
`~/.config/openchess/chess.conf`（安装后）。外观修改会自动保存。

```ini
board = icy_sea      # assets/themes.txt 中的任意键
pieces = cases       # assets/themes.txt 中的任意键
animation = arcade   # arcade | slide | fade | none
board_size = 88      # 格子大小（44-150）
engine = /opt/homebrew/bin/stockfish   # 可选
```

也可用环境变量覆盖：`OPENCHESS_ASSETS`（资源目录）、`OPENCHESS_CONFIG`
（配置文件路径）。

## 目录结构

```
src/        C 源码（board、move、pgn、fen、ai、net、themes、paths、gui、main）
assets/     boards/、pieces/ 与 themes.txt（随仓库提供）
tests/      单元测试与冒烟测试
docs/       中英双语文档
scripts/    资源导入与 .app/.dmg 辅助脚本
```

## 许可证与素材

源代码：MIT（见 `LICENSE`）。棋盘与棋子图片版权归 chess.com 所有，由第三方仓库
收集 —— 详见 [`ATTRIBUTIONS.md`](ATTRIBUTIONS.md)。
