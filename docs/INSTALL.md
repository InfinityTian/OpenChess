# Installing OpenChess / 安装 OpenChess

Terminal install for Linux/macOS and the macOS app bundle. /
Linux/macOS 终端安装与 macOS 应用包。

---

## English

### Terminal install (Linux and macOS)

```sh
./install.sh                     # installs into ~/.local
PREFIX=/usr/local ./install.sh   # install into /usr/local (may need sudo)
make install PREFIX=$HOME/.local # same, via make
```

The install layout:

```
$PREFIX/bin/openchess
$PREFIX/share/openchess/assets/            (boards, pieces, themes.txt)
$PREFIX/share/openchess/chess.conf.example
```

Add the binary directory to your `PATH` if it is not already:

```sh
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc   # or ~/.bashrc
```

The program finds its assets relative to the executable, so it can be launched
from any directory. Settings live in `~/.config/openchess/chess.conf`.

### Uninstall

```sh
./uninstall.sh                 # remove the program and assets
./uninstall.sh --purge         # also remove ~/.config/openchess (settings)
PREFIX=/usr/local ./uninstall.sh
```

### macOS application bundle and disk image

Build a self-contained bundle and a distributable disk image:

```sh
make dmg      # -> dist/OpenChess.app and dist/OpenChess.dmg
```

- `dist/OpenChess.app` contains the executable in `Contents/MacOS` and the
  assets in `Contents/Resources`; no separate install is needed.
- `dist/OpenChess.dmg` is a compressed image you can share. Open it and drag
  `OpenChess.app` into `/Applications`.

The app is **not code-signed or notarized**. macOS adds a quarantine flag when
the `.dmg` is downloaded, so on first launch Gatekeeper may block it. To allow
it, either right-click the app and choose **Open**, or clear the flag before
opening the image:

```sh
sudo xattr -r -d com.apple.quarantine /path/to/OpenChess.dmg
```

If you already dragged `OpenChess.app` into `/Applications`, clear the copy
instead:

```sh
sudo xattr -r -d com.apple.quarantine /Applications/OpenChess.app
```

### Windows

**Native (MSYS2 / MinGW-w64).** Install the MSYS2 UCRT64 toolchain and SDL2
packages, then build from the repository root:

```sh
make CC=gcc        # -> openchess.exe
```

Run `openchess.exe` from the repository root (so `assets/` is found) or copy it
next to the `assets/` folder. Get the Windows build of Stockfish from
<https://stockfishchess.org/download/> and put `stockfish.exe` on `PATH`, or set
`engine =` in `%APPDATA%\openchess\chess.conf`. The full `pacman` command is in
[`BUILD.md`](BUILD.md).

**WSL2 (Windows 11 + WSLg).** Use the Linux build; the window appears through
WSLg:

```sh
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev stockfish
make
./openchess
```

### Where files live

| Kind | Development | Installed | macOS bundle |
| --- | --- | --- | --- |
| Assets | `./assets` | `$PREFIX/share/openchess/assets` | `Contents/Resources/assets` |
| Config | `./chess.conf` | `~/.config/openchess/chess.conf` | `~/.config/openchess/chess.conf` |
| Exported games | `~/.local/share/openchess/games` | same | same |

Overrides: `OPENCHESS_ASSETS` (assets directory) and `OPENCHESS_CONFIG`
(config file path). `XDG_DATA_HOME` changes where exported PGN games are saved.

### Troubleshooting

- **"Networking unavailable"** — SDL2_net was not found at build time; install
  it and rebuild (`make clean && make`).
- **"Stockfish not found"** — install Stockfish or set `engine = /path/to/stockfish`
  in `chess.conf`.
- **No text / missing glyphs** — install a font such as Arial Unicode (macOS) or
  DejaVu Sans (Linux).
- **Assets missing** — ensure the assets directory resolved next to the binary,
  or set `OPENCHESS_ASSETS`.

---

## 简体中文

### 终端安装（Linux 与 macOS）

```sh
./install.sh                     # 安装到 ~/.local
PREFIX=/usr/local ./install.sh   # 安装到 /usr/local（可能需要 sudo）
make install PREFIX=$HOME/.local # 通过 make 完成同样操作
```

安装后的目录结构：

```
$PREFIX/bin/openchess
$PREFIX/share/openchess/assets/            (棋盘、棋子、themes.txt)
$PREFIX/share/openchess/chess.conf.example
```

如果二进制目录不在 `PATH` 中，请添加：

```sh
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.zshrc   # 或 ~/.bashrc
```

程序会相对可执行文件查找资源，因此可在任意目录启动。设置保存在
`~/.config/openchess/chess.conf`。

### 卸载

```sh
./uninstall.sh                 # 删除程序与资源
./uninstall.sh --purge         # 同时删除 ~/.config/openchess（设置）
PREFIX=/usr/local ./uninstall.sh
```

### macOS 应用包与磁盘映像

生成自包含的应用包与可分发的磁盘映像：

```sh
make dmg      # 生成 dist/OpenChess.app 与 dist/OpenChess.dmg
```

- `dist/OpenChess.app` 的可执行文件位于 `Contents/MacOS`，资源位于
  `Contents/Resources`，无需另行安装。
- `dist/OpenChess.dmg` 为压缩映像，打开后将 `OpenChess.app` 拖入
  `/Applications` 即可。

应用**未做代码签名，也未公证**。下载 `.dmg` 时 macOS 会添加隔离属性，首次启动可能被
Gatekeeper 拦截。允许方式：右键点击应用选择 **打开**，或在打开映像前先清除隔离属性：

```sh
sudo xattr -r -d com.apple.quarantine /path/to/OpenChess.dmg
```

若已将 `OpenChess.app` 拖入 `/Applications`，则改为清除该副本：

```sh
sudo xattr -r -d com.apple.quarantine /Applications/OpenChess.app
```

### Windows

**原生（MSYS2 / MinGW-w64）**：安装 MSYS2 UCRT64 工具链与 SDL2 包，然后在仓库根目录构建：

```sh
make CC=gcc        # -> openchess.exe
```

请在仓库根目录运行 `openchess.exe`（以便找到 `assets/`），或将其与 `assets/`
放在同一目录。从 <https://stockfishchess.org/download/> 下载 Windows 版
Stockfish，将 `stockfish.exe` 放入 `PATH`，或在
`%APPDATA%\openchess\chess.conf` 中设置 `engine =`。完整 `pacman` 命令见
[`BUILD.md`](BUILD.md)。

**WSL2（Windows 11 + WSLg）**：直接使用 Linux 构建，窗口通过 WSLg 显示：

```sh
sudo apt install build-essential pkg-config \
    libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-net-dev stockfish
make
./openchess
```

### 文件位置

| 类型 | 开发环境 | 安装后 | macOS 应用包 |
| --- | --- | --- | --- |
| 资源 | `./assets` | `$PREFIX/share/openchess/assets` | `Contents/Resources/assets` |
| 配置 | `./chess.conf` | `~/.config/openchess/chess.conf` | `~/.config/openchess/chess.conf` |
| 导出对局 | `~/.local/share/openchess/games` | 同上 | 同上 |

可用环境变量覆盖：`OPENCHESS_ASSETS`（资源目录）、`OPENCHESS_CONFIG`
（配置文件路径）。导出 PGN 的位置可通过 `XDG_DATA_HOME` 改变。

### 常见问题

- **提示 “Networking unavailable”** —— 构建时未找到 SDL2_net；安装后重新构建
  （`make clean && make`）。
- **提示 “Stockfish not found”** —— 安装 Stockfish，或在 `chess.conf` 中设置
  `engine = /path/to/stockfish`。
- **文字缺失/字形异常** —— 安装字体，如 macOS 的 Arial Unicode 或 Linux 的
  DejaVu Sans。
- **资源缺失** —— 确认可执行文件旁的资源目录存在，或设置 `OPENCHESS_ASSETS`。
