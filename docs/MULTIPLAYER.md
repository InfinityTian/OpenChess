# Local Multiplayer / 本地多人

Two OpenChess instances over TCP. / 通过 TCP 连接两个 OpenChess 实例。

---

## English

Local Multiplayer connects two running copies of OpenChess over TCP. It works on
a single machine (loopback) or across a LAN. It requires that OpenChess was built
with **SDL2_net** (the menu entry is greyed out otherwise).

### Hosting a game

1. From the welcome menu choose **Local Multiplayer**.
2. Set the **Port** (default `7777`).
3. Select **Host** and press **Enter**. The status shows
   "Waiting for opponent...". The host plays **White**.
4. Once the opponent connects, the game starts automatically.

### Joining a game

1. Choose **Local Multiplayer**.
2. Set the **Opponent address** and **Port**:
   - Same machine: `127.0.0.1`.
   - Another machine: the host's LAN IP (for example `192.168.1.20`).
3. Select **Join** and press **Enter**. The joiner plays **Black**.

The board is automatically oriented so your colour is at the bottom.

### Controls and rules

- Moves are made with the mouse or with SAN text (your side only, on your turn);
  every move is sent to the peer automatically.
- **Undo** and **Restart** are disabled in multiplayer to keep the two sides in
  sync.
- If the connection drops, a "Opponent disconnected" message appears and you
  return to the menu.

### Networking notes

- The host must allow inbound TCP on the chosen port; check your firewall.
- Both sides start from the standard position.
- The transport is plain text, newline-delimited: `HELLO`, `FEN`, `MOVE <uci>`,
  and `BYE`. Moves are exchanged in UCI form (`e2e4`, `e7e8q`).

### Troubleshooting

- **Menu entry greyed out** — rebuild with SDL2_net installed.
- **"Could not connect"** — check the address/port and that the host is waiting;
  verify the firewall allows the port.
- **"Could not listen on port"** — the port is in use; choose another (for
  example `7778`) on both sides.

---

## 简体中文

本地多人模式通过 TCP 连接两个正在运行的 OpenChess。可用于同一台机器（回环）或
局域网。该模式要求 OpenChess 在构建时包含 **SDL2_net**（否则菜单项为灰色）。

### 创建主机（Host）

1. 在欢迎菜单选择 **Local Multiplayer**。
2. 设置 **Port**（默认 `7777`）。
3. 选择 **Host** 并按 **Enter**，状态显示 “Waiting for opponent...”。主机执
   **白方**。
4. 对手连接成功后自动开始对局。

### 加入对局（Join）

1. 选择 **Local Multiplayer**。
2. 设置 **Opponent address**（对手地址）与 **Port**：
   - 同一台机器：`127.0.0.1`。
   - 另一台机器：主机的局域网 IP（例如 `192.168.1.20`）。
3. 选择 **Join** 并按 **Enter**。加入方执 **黑方**。

棋盘会自动朝向，使你的颜色位于下方。

### 操作与规则

- 可用鼠标或 SAN 文本走子（仅限你的回合与你的一方）；每一步都会自动发送给对手。
- 多人模式下 **Undo** 与 **Restart** 被禁用，以保持双方同步。
- 连接断开时会提示 “Opponent disconnected”，并返回菜单。

### 网络说明

- 主机需允许所选端口的入站 TCP，请检查防火墙。
- 双方均从标准初始局面开始。
- 传输为纯文本、以换行分隔：`HELLO`、`FEN`、`MOVE <uci>`、`BYE`。着法使用 UCI
  形式（`e2e4`、`e7e8q`）。

### 常见问题

- **菜单项为灰色** —— 需在安装 SDL2_net 后重新构建。
- **提示 “Could not connect”** —— 检查地址/端口，确认主机正在等待，并确认防火墙
  放行该端口。
- **提示 “Could not listen on port”** —— 端口被占用；请双方改用其他端口
  （例如 `7778`）。
