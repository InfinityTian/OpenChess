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

### Board flipping

- **Ctrl+F** flips the board manually.
- In Singleplayer and Local Multiplayer the board is automatically oriented to
  your colour at the start.

### Keyboard reference

| Key | Action |
| --- | --- |
| `Enter` | Reveal SAN box / submit typed move |
| `Esc` | Cancel entry, clear selection, or go back |
| `Ctrl+B` / `Ctrl+P` / `Ctrl+M` | Cycle board / pieces / animation |
| `Ctrl+F` | Flip board |
| `Ctrl+U` | Undo (not in multiplayer) |
| `Ctrl+R` | Restart (not in multiplayer) |
| `Ctrl+C` | Copy FEN |
| `Ctrl+Q` | Quit |
| Arrows / `W` `S` | Menu navigation |
| `Tab`, `1` `2` `3` | Appearance tabs / host-join fields |

### Configuration

`chess.conf` (development) or `~/.config/openchess/chess.conf` (installed):

```ini
board = icy_sea      # any key from assets/themes.txt
pieces = cases       # any key from assets/themes.txt
animation = arcade   # arcade | slide | fade | none
engine = /opt/homebrew/bin/stockfish   # optional
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

### 棋盘翻转

- **Ctrl+F** 手动翻转。
- 单人/本地多人开局时会自动朝向你的颜色。

### 键盘速查

| 按键 | 作用 |
| --- | --- |
| `Enter` | 显示 SAN 输入框 / 提交着法 |
| `Esc` | 取消输入、清除选择或返回 |
| `Ctrl+B` / `Ctrl+P` / `Ctrl+M` | 循环切换棋盘 / 棋子 / 动画 |
| `Ctrl+F` | 翻转棋盘 |
| `Ctrl+U` | 悔棋（多人模式不可用） |
| `Ctrl+R` | 重新开始（多人模式不可用） |
| `Ctrl+C` | 复制 FEN |
| `Ctrl+Q` | 退出 |
| 方向键 / `W` `S` | 菜单导航 |
| `Tab`、`1` `2` `3` | 外观标签页 / 主机-加入字段 |

### 配置

`chess.conf`（开发环境）或 `~/.config/openchess/chess.conf`（安装后）：

```ini
board = icy_sea      # assets/themes.txt 中的任意键
pieces = cases       # assets/themes.txt 中的任意键
animation = arcade   # arcade | slide | fade | none
engine = /opt/homebrew/bin/stockfish   # 可选
```

可用环境变量覆盖：`OPENCHESS_ASSETS`、`OPENCHESS_CONFIG`。
