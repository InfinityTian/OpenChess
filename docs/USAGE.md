# Using OpenChess / 使用 OpenChess

Menu, gameplay, controls and configuration. / 菜单、玩法、操作与配置。

---

## English

### The welcome menu

On launch you see the mode list. Use the **arrow keys** (or `W`/`S`) to move and
**Enter**/Space (or the mouse) to choose:

- **Singleplayer (vs AI)** — play Stockfish.
- **Analysis (both sides)** — one person controls both colours.
- **Local Multiplayer** — connect to another instance (needs SDL2_net).
- **Appearance (boards & pieces)** — the visual style picker.
- **Quit** — exit (`Ctrl+Q` works anywhere).

A greyed-out entry is unavailable (for example Local Multiplayer without
SDL2_net).

### Moving pieces

- **Mouse**: click a piece, then click a highlighted destination. You can also
  drag. Legal destinations show as dots (rings for captures); the last move and
  check are highlighted.
- **SAN text** *(all modes, but only for the side you control)*: press **Enter**
  to reveal the move box, type in algebraic notation, then press **Enter** to
  play. **Esc** cancels. Examples: `e4`, `Nf3`, `exd5`, `O-O`, `O-O-O`, `e8=Q+`.
  - Analysis allows both sides; Singleplayer and Local Multiplayer only accept
    moves for your own side on your turn.

### Board annotations

- **Right-click** a square to toggle a coloured highlight on it.
- **Right-drag** from one square to another to draw a solid arrow.
- Hold **Ctrl** (green) or **Alt** (blue) while right-clicking/dragging to
  change the colour; the default is amber.
- Annotations are cleared when you make a move, press **Esc**, or left-click the
  board.

### Analysis mode

- Control White and Black with full legality; use it to try lines or set up
  positions.
- **FEN** (right panel): click the field and type/paste a FEN, then press
  **Enter** or click **Load** to apply it. **Copy** (or **Ctrl+C**) copies the
  current position to the clipboard.
- **Undo** (Ctrl+U) steps back one ply; **Restart** (Ctrl+R) resets to the start.

### Singleplayer

1. Choose the menu entry, then pick **Play as** (White/Black) and
   **Difficulty** (Easy / Medium / Hard).
2. **Start** begins the game. The board is oriented to your colour.
3. The engine replies automatically; a "thinking..." note appears while it
   searches.
4. **Undo** takes back to your previous turn.

If Stockfish is missing, install it (see `BUILD.md`) or set `engine =` in the
config file.

### Appearance picker

- Open from the menu, or the in-game **Styles** button.
- Tabs: **Boards**, **Pieces**, **Animation**.
  - **Arrows** move the selection (a grid page follows), **Tab** or **1 / 2 / 3**
    switches tabs, **Enter** applies, **Esc** goes back.
  - Boards and pieces show live thumbnails; the Animation tab previews each
    effect.
- Your choice is applied immediately and saved to `chess.conf`.

### Board flipping and resizing

- **Ctrl+F** flips the board manually.
- In Singleplayer and Local Multiplayer the board is automatically oriented to
  your colour at the start.
- **Resize / magnify**: the whole UI (board, pieces, panel text, buttons and
  menus) scales together. Drag the grip in the board's bottom-right corner, or
  resize the window normally. The resulting magnification is saved in
  `chess.conf` as `board_size` (the equivalent square size, 44–150).

### Settings

Open **Settings** from the welcome menu or with **Ctrl+E**. It has three tabs
(**Tab** or **1 / 2 / 3** to switch, **Esc** to go back):

- **Engine** – pick an engine detected on your `PATH` or type a custom
  executable path, and set the analysis options (Lines, Threads, Hash, Time,
  Depth).
- **Gameplay** – cycle the **Board**, **Pieces** and **Animation** styles, or
  open the full **Appearance** picker.
- **Audio & Video** – toggle **Sound** (move/capture/castle/check/promote/game-end
  sounds) and set **Max FPS** (`vsync`, or `60 / 120 / 144 / 240 / 320`).

Use **Up/Down** to select a row and **Left/Right** (or the `-`/`+` buttons) to
change it. Everything is saved to `chess.conf` (`sound`, `max_fps`).

### Engine and evaluation (Analysis)

- Open **Settings → Engine** from the welcome menu or with **Ctrl+E** while
  playing. Pick an engine detected on your `PATH`, or type a custom executable
  path (click the field to edit, Enter to apply). The choice is saved as `engine`.
- In Analysis the selected engine analyses the current position continuously: a
  vertical **evaluation bar** is drawn to the left of the board and the numeric
  score and search depth appear in the panel. It restarts after every move,
  undo, restart or FEN load.
- **Engine lines**: the panel has a **0–5 slider**. `0` closes the engine; `1–5`
  selects how many MultiPV lines are shown (each line lists the score and the
  start of the principal variation in SAN).
- **Engine arrows**: the **Arrows** checkbox next to the slider (on by default)
  draws each line's first move as a green arrow on the board.
- **Engine controls** (Settings → Engine): **Lines** (same as the slider),
  **Threads** (CPU cores), **Hash** (MB), **Time** (ms, `0` = unlimited) and
  **Depth** (`0` = unlimited). Click the `-`/`+` buttons (or use **Left/Right**
  after focusing a control) to adjust, and **Up/Down** to pick an engine. All are
  saved to `chess.conf`.

### Export as PGN

- Press **Ctrl+S** or click **PGN** (only when at least one move has been made).
- Type a file name and press **Enter**; the game is written to
  `~/.local/share/openchess/games/<name>.pgn` (honouring `XDG_DATA_HOME`) with
  `Event/Site/Date/Round/White/Black/Result` headers and the movetext.
- The status message shows the full path. **Esc** cancels.

### Keyboard reference

| Key | Action |
| --- | --- |
| `Enter` | Reveal SAN box / submit typed move |
| `Esc` | Cancel entry, clear selection, or go back |
| `Ctrl+B` / `Ctrl+P` / `Ctrl+M` | Cycle board / pieces / animation |
| `Ctrl+E` | Settings |
| `Ctrl+S` | Export game as PGN |
| `Ctrl+F` | Flip board |
| `Ctrl+U` | Undo (not in multiplayer) |
| `Ctrl+R` | Restart (not in multiplayer) |
| `Ctrl+C` | Copy FEN |
| `Ctrl+Q` | Quit |
| Board corner drag | Resize the board |
| Arrows / `W` `S` | Menu navigation |
| `Tab`, `1` `2` `3` | Appearance tabs / host-join fields |

### Configuration

`chess.conf` (development) or `~/.config/openchess/chess.conf` (installed):

```ini
board = icy_sea      # any key from assets/themes.txt
pieces = cases       # any key from assets/themes.txt
animation = arcade   # arcade | slide | fade | none
board_size = 88      # square size in points (44-150)
engine = /opt/homebrew/bin/stockfish   # optional
engine_multipv = 1   # engine lines 0-5 (0 = engine off)
engine_threads = 1   # CPU cores
engine_hash = 16     # transposition table MB
engine_time = 0      # per-move / analysis time limit in ms (0 = unlimited)
engine_depth = 0     # depth limit (0 = unlimited)
engine_arrows = 1    # green engine arrow on the board (0/1)
sound = 1            # play move sounds (0/1)
max_fps = 0          # 0 = vsync, or 60/120/144/240/320
```

Environment overrides: `OPENCHESS_ASSETS`, `OPENCHESS_CONFIG`.

---

## 简体中文

### 欢迎菜单

启动后显示模式列表。使用**方向键**（或 `W`/`S`）移动，**回车**/空格（或鼠标）选择：

- **单人（对战 AI）** —— 与 Stockfish 对弈。
- **分析（双方）** —— 一人控制双方。
- **本地多人** —— 连接另一个实例（需要 SDL2_net）。
- **外观（棋盘与棋子）** —— 可视化风格选择器。
- **退出** —— 结束程序（任意界面可用 `Ctrl+Q`）。

灰色条目表示不可用（例如未安装 SDL2_net 时的本地多人）。

### 走子

- **鼠标**：点击棋子，再点击高亮的目标格；也支持拖拽。合法目标显示为圆点（吃子
  为圆环）；上一步与将军会被高亮。
- **SAN 文本** *（所有模式，但仅限你控制的一方）*：按 **Enter** 显示输入框，
  输入代数记谱，再按 **Enter** 落子，**Esc** 取消。示例：`e4`、`Nf3`、
  `exd5`、`O-O`、`O-O-O`、`e8=Q+`。
  - 分析模式双方均可输入；单人/本地多人仅在你回合、输入己方着法时有效。

### 棋盘标注

- **右键**点击格子可切换圆圈标记。
- **右键拖动**可从一格到另一格画箭头。
- 按住 **Ctrl**（绿色）或 **Alt**（蓝色）再右键/拖动可改变颜色，默认为琥珀色。
- 走子、按 **Esc** 或左键点击棋盘会清除标注。

### 分析模式

- 可合法地控制白方与黑方，适合试走线路或摆谱。
- **FEN**（右侧面板）：点击输入框，输入/粘贴 FEN，按 **Enter** 或点击 **Load**
  应用。**Copy**（或 **Ctrl+C**）将当前局面复制到剪贴板。
- **Undo**（Ctrl+U）悔一步；**Restart**（Ctrl+R）回到初始局面。

### 单人模式

1. 进入后选择**执子方**（白/黑）与**难度**（Easy / Medium / Hard）。
2. 点击 **Start** 开始，棋盘会自动朝向你的颜色。
3. 引擎自动应着；搜索时显示 “thinking...”。
4. **Undo** 会退回到你上一次的回合。

若缺少 Stockfish，请安装（见 `BUILD.md`）或在配置文件中设置 `engine =`。

### 外观选择器

- 可从菜单或游戏内 **Styles** 按钮打开。
- 标签页：**Boards**（棋盘）、**Pieces**（棋子）、**Animation**（动画）。
  - **方向键**移动选择（会自动翻页），**Tab** 或 **1 / 2 / 3** 切换标签，
    **Enter** 应用，**Esc** 返回。
  - 棋盘与棋子显示实时缩略图；动画标签会逐项预览效果。
- 选择即时生效并保存到 `chess.conf`。

### 棋盘翻转与缩放

- **Ctrl+F** 手动翻转。
- 单人/本地多人开局时会自动朝向你的颜色。
- **缩放 / 放大**：整个界面（棋盘、棋子、面板文字、按钮与菜单）会一起缩放。
  可拖动棋盘右下角手柄，或直接缩放系统窗口。缩放比例以 `board_size`（等效格子大小，
  44–150）保存到 `chess.conf`。

### 设置（Settings）

可从欢迎菜单或游戏中按 **Ctrl+E** 打开 **Settings**。它有三个标签页（**Tab** 或
**1 / 2 / 3** 切换，**Esc** 返回）：

- **Engine**：选择 `PATH` 中检测到的引擎或自定义可执行文件路径，并设置分析选项
  （Lines、Threads、Hash、Time、Depth）。
- **Gameplay**：循环切换**棋盘**、**棋子**、**动画**样式，或打开完整的外观选择器。
- **Audio & Video**：切换 **Sound**（走子/吃子/王车易位/将军/升变/终局音效）与
  **Max FPS**（`vsync`，或 `60 / 120 / 144 / 240 / 320`）。

用 **上下键**选择行，**左右键**（或 `-`/`+` 按钮）修改。全部保存到 `chess.conf`
（`sound`、`max_fps`）。

### 引擎与评估（分析模式）

- 可从欢迎菜单或游戏中按 **Ctrl+E** 打开 **Settings → Engine**。选择在 `PATH` 中
  检测到的引擎，或输入自定义可执行文件路径（点击输入框编辑，Enter 应用）。选择
  保存为 `engine`。
- 分析模式下所选引擎会持续分析当前局面：棋盘左侧显示垂直**评估条**，面板中显示
  分数与搜索深度。每步走子、悔棋、重开或载入 FEN 后都会重新分析。
- **引擎箭头**：勾选 “Arrows” 可用绿色箭头在棋盘上标出每条线路的首选着法（默认开）。
- **引擎多线路**：面板中有一个 **0–5 滑块**。`0` 表示关闭引擎；`1–5` 表示显示的
  MultiPV 线路数量（每条显示分数与主变着法的 SAN 开头）。
- **引擎控制**（Settings → Engine）：**Lines**（同上）、**Threads**（CPU 核心）、
  **Hash**（MB）、**Time**（毫秒，`0` 为不限）、**Depth**（`0` 为不限）。点击
  `-`/`+` 按钮（或聚焦控制项后用**左右键**）调整，**上下键**选择引擎。所有设置都会
  保存到 `chess.conf`。

### 导出 PGN

- 按 **Ctrl+S** 或点击 **PGN** 按钮（至少走过一步时可用）。
- 输入文件名后按 **Enter**，对局会写入
  `~/.local/share/openchess/games/<name>.pgn`（遵循 `XDG_DATA_HOME`），包含
  `Event/Site/Date/Round/White/Black/Result` 头部与着法文本。
- 状态栏会显示完整路径，**Esc** 取消。

### 键盘速查

| 按键 | 作用 |
| --- | --- |
| `Enter` | 显示 SAN 输入框 / 提交着法 |
| `Esc` | 取消输入、清除选择或返回 |
| `Ctrl+B` / `Ctrl+P` / `Ctrl+M` | 循环切换棋盘 / 棋子 / 动画 |
| `Ctrl+E` | 设置 |
| `Ctrl+S` | 导出对局为 PGN |
| `Ctrl+F` | 翻转棋盘 |
| `Ctrl+U` | 悔棋（多人模式不可用） |
| `Ctrl+R` | 重新开始（多人模式不可用） |
| `Ctrl+C` | 复制 FEN |
| `Ctrl+Q` | 退出 |
| 拖动棋盘角 | 缩放棋盘 |
| 方向键 / `W` `S` | 菜单导航 |
| `Tab`、`1` `2` `3` | 外观标签页 / 主机-加入字段 |

### 配置

`chess.conf`（开发环境）或 `~/.config/openchess/chess.conf`（安装后）：

```ini
board = icy_sea      # assets/themes.txt 中的任意键
pieces = cases       # assets/themes.txt 中的任意键
animation = arcade   # arcade | slide | fade | none
board_size = 88      # 格子大小（44-150）
engine = /opt/homebrew/bin/stockfish   # 可选
engine_multipv = 1   # 引擎线路 0-5（0 = 关闭引擎）
engine_threads = 1   # CPU 核心数
engine_hash = 16     # 置换表 MB
engine_time = 0      # 每步/分析时限（毫秒，0 = 不限）
engine_depth = 0     # 深度限制（0 = 不限）
engine_arrows = 1    # 棋盘绿色引擎箭头（0/1）
sound = 1            # 走子音效（0/1）
max_fps = 0          # 0 = vsync，或 60/120/144/240/320
```

可用环境变量覆盖：`OPENCHESS_ASSETS`、`OPENCHESS_CONFIG`。
