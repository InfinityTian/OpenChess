# OpenChess 3.0 P2P

## 使用方式

1. 双方打开首页的 **P2P 好友对弈**，无需注册或登录。
2. 房主选择时限和白方／黑方／随机，点击 **创建邀请**。
3. 等待提示邀请就绪，点击 **复制邀请／回应**，通过自己使用的通信软件把完整内容发给好友。
4. 好友复制邀请，在自己的 OpenChess 中点击 **从剪贴板导入**；无需先创建另一份邀请。
5. 好友等待回应就绪，点击 **复制邀请／回应**，把完整回应发回房主。
6. 房主复制回应并点击 **从剪贴板导入**。连接成功后自动开局，双方显示相同的时限与相反的执子颜色。

邀请以 `OC-P2P1:` 开头，是一段完整的连接信息，不是短房间号。邀请包含连接地址、临时凭据和证书指纹，只发送给预期对手；过期、关闭或重置连接后请重新交换。连接协商最多等待十分钟。

- 默认 10+0，可选不限时及 1+0、2+1、3+0、3+2、5+0、5+3、10+0、10+5、15+10、30+0。
- 时限和颜色由房主在创建前设置。重赛沿用时限并交换颜色，双方点击“再来一局”后开始。
- 房主统一管理计时，双方检查走子合法性和步数。返回首页或打开设置时对局继续，可点击右上角返回。
- 支持认输、提议和接受和棋、连接内的胜／和／负比分、本地棋谱历史和赛后分析。此版本没有公开匹配、可信在线 ELO、在线玩家目录、离线私信或跨设备同步。
- 连接中断或三十秒无回应会停止对局并保留棋盘供复盘；不自动判负，不支持断线续局，需要新邀请重新开局。
- P2P 对局不改变对局 ELO 或 Puzzle Rating；本地解题、每日题与分析数据继续保留。

## 网络

游戏和连接协商不使用 OpenChess 服务器。WebRTC DataChannel 使用 DTLS 加密，双方手动交换完整的 SDP；没有隐含的信令服务。

默认使用 Cloudflare 的公共 STUN 地址 `stun:stun.cloudflare.com:3478`，辅助发现网络地址。它不是游戏服务器，也不转发对局。可在创建／导入前编辑或清空地址；清空时仅收集本地地址，适合同一局域网或可直接访问的网络。

严格 NAT、防火墙或 UDP 限制可能导致连接失败，不能保证所有网络直连。可自行配置 UDP TURN 地址 `turn:用户名:密码@主机:端口`，或者在虚拟局域网中使用现有局域网对弈。当前 libjuice 构建不提供 TURN TCP/TLS 中继。不内置 TURN 账号或付费服务。

官方参考：[libdatachannel](https://github.com/paullouisageneau/libdatachannel/tree/v0.24.2)、[Cloudflare STUN](https://developers.cloudflare.com/realtime/sfu/get-started/connection-patterns/)。

## Build

Install CMake, Git, a C++17 compiler and OpenSSL development headers, alongside the existing SDL dependencies. On Windows use MSYS2 UCRT64 CMake/Ninja/GCC and OpenSSL. On macOS use Homebrew CMake and OpenSSL.

```sh
make p2p-deps
make -j3
make test
```

The bootstrap pins libdatachannel 0.24.2 to commit `4e4f4892dccb2a57fe3a490d0c9d958de4244e74` and its submodules. Dependencies build into `.cache/p2p`; no system installation is performed. The build remains available without WebRTC, with a clear unavailable message; official releases always build and bundle WebRTC and its licenses. Run `make p2p-deps` and `make` as separate commands so dependency detection is refreshed.

## English quick start

Open **P2P with a friend** on both computers. The creator chooses time/color, creates and copies an invitation, and sends it privately. The friend imports it from the clipboard, then copies and returns the generated response. The creator imports that response. The encrypted game starts automatically. No account or OpenChess game/signaling server is needed. STUN is optional; restricted networks may still need a user-supplied UDP TURN relay or virtual LAN.

Games are unrated. The creator referees clocks, both sides validate moves, and accepted rematches swap colors. Navigation preserves active games. Disconnections stop the game and keep its board for analysis; reconnection/resumption and centralized account services are not part of this release.
