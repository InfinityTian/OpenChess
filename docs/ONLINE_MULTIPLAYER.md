# Online Multiplayer — Design & Implementation Plan

Status: **plan** (not implemented). This document describes how to grow the
existing LAN **Local Multiplayer** into internet **Online Multiplayer**.

---

## 1. Where we are today

- `src/net.c` / `src/net.h`: a tiny newline-delimited TCP transport on
  **SDL2_net**. Non-blocking poll (`net_poll`), one-line sends (`net_send`).
- `src/gui.c` (`SCENE_HOSTJOIN`, `enter_local_game`, `local_tick`): a Host/Join
  screen where the user types an IP and port. Host plays White, joiner Black.
- Wire protocol (plain text, one message per line):
  - `HELLO CHESS1 <white|black>` — role/side handshake.
  - `FEN <fen>` — initial position.
  - `MOVE <uci>` — a move (`e2e4`, `e7e8q`).
  - `BYE` — intentional close.
- Limitations for the internet: manual addresses, no NAT traversal, no
  rendezvous/matchmaking, no TLS, no reconnection, no protocol version, no
  server-side validation, no clocks, no rematch, no spectator/reconnect.

Good news: chess is **turn-based and tiny**. Bandwidth is bytes per move and
latency is irrelevant, so we do **not** need peer-to-peer UDP or hole punching to
ship a great experience. An outbound-only **relay** solves NAT, CGNAT, and
firewalls cleanly.

---

## 2. Goals & non-goals

### Goals
- Connect two players from anywhere with **no port forwarding**.
- A short **room code** instead of an IP address.
- **Server-validated** moves (never trust the client) so games are cheat-proof.
- Reconnect after a brief drop; graceful handling of resignation/timeouts.
- Optional, additive features: clocks, rematch, chat, spectators, rating.

### Non-goals (initially)
- Real-time low-latency twitch play (irrelevant for chess).
- Full tournament/rating infrastructure in v1.
- Mobile/native clients beyond the desktop SDL app.

---

## 3. Architecture

```
┌──────────┐        wss/https        ┌───────────────┐
│ Client A │ ───────────────────────▶│  Lobby/Relay  │
│  (SDL)   │◀─────── relayed ────────│    Server     │
└──────────┘        messages         │  (authoritative)
┌──────────┐                          │
│ Client B │ ───────────────────────▶│
└──────────┘                          └───────┬───────┘
                                              │
                                       ┌──────▼──────┐
                                       │  Postgres   │
                                       │ (rooms,     │
                                       │  results)   │
                                       └─────────────┘
```

- **Clients only make outbound connections** to one well-known server. No NAT
  configuration ever.
- The **server relays** messages between the two seats of a room and is the
  single source of truth: it holds the `Board`, validates legality, tracks the
  clock, and decides the result.
- The client-side board/move engine (`src/board.c`, `src/move.c`, `src/fen.c`)
  is already pure C and can be **compiled into the server** to avoid a second
  rules implementation. Alternatively use a small independent validator.

### Transport choice
Move from raw TCP/SDL_net to **WebSocket (`wss://`)** for online mode:
- Works through every HTTP proxy/firewall and terminates cleanly with TLS.
- Well-supported in C (e.g. `libwebsockets`, `libcurl` WS, or a tiny embeddable
  client) and trivially in Go/Node/Rust servers.
- Keep SDL2_net for **LAN** mode (local play should keep working offline); add a
  transport abstraction so the GUI does not care which one is active.

### Server language
Recommended: **Go** (single static binary, first-class concurrency, easy WS +
Postgres, trivial Docker/Fly.io deploy). Alternative: **C** to reuse the engine
and keep the repo single-language, at the cost of more boilerplate (still very
doable with POSIX sockets or `libwebsockets`). Decide before Phase 2; the client
protocol is language-agnostic.

---

## 4. Protocol (v2)

JSON over WebSocket, one object per frame, with a `type` discriminator. Keep the
existing line protocol as the LAN fallback. Every frame carries `v` (protocol
version) at connect time.

### Client → Server
| type | fields | meaning |
| --- | --- | --- |
| `hello` | `v`, `token?`, `nick` | open session, resume if `token` known |
| `create` | `color?`, `clock?` | create a room, returns code |
| `join` | `code` | join a room by code |
| `move` | `uci`, `ply` | propose a move |
| `resign` | — | resign |
| `draw_offer` / `draw_accept` / `draw_decline` | — | draw negotiation |
| `rematch` | — | request a new game in the same room |
| `chat` | `text` | in-game chat (rate-limited) |
| `ping` | `t` | heartbeat / RTT |

### Server → Client
| type | fields | meaning |
| --- | --- | --- |
| `welcome` | `token`, `server_time` | session id for reconnect |
| `room` | `code`, `color`, `white`, `black`, `clock` | room state |
| `start` | `fen`, `side`, `clock`, `opponent` | game begins |
| `move` | `uci`, `ply`, `san`, `clock`, `fen?` | accepted move (authoritative) |
| `reject` | `reason` | illegal/out-of-turn move (keeps client honest) |
| `state` | `fen`, `ply`, `clock`, `result?` | resync after reconnect |
| `gameover` | `result`, `reason` | checkmate/stalemate/resign/time/draw |
| `opponent` | `connected`, `nick` | presence changes |
| `chat` | `from`, `text` | relayed chat |
| `pong` | `t` | heartbeat reply |

### Rules
- Server assigns side on `create`/`join` (host = White by default, optional
  random).
- Server validates **every** `move` against the authoritative board; a client
  may still pre-validate locally for instant feedback, but the server's `move`
  echo is the truth. On mismatch send `reject` + `state`.
- Clocks are computed server-side (client interpolates for display only).
- Protocol is versioned; unknown fields are ignored, unknown `type` is a no-op.

---

## 5. Server components

1. **HTTP API** (TLS):
   - `POST /v1/session` → anonymous token (device id / OAuth later).
   - `POST /v1/rooms` → `{code}`.
   - `GET  /healthz`, `GET /metrics`.
2. **WebSocket endpoint** `/v1/ws` — the game loop.
3. **Room manager** — in-memory map `code → Room{seats, board, clock, tokens}`;
   persist results asynchronously.
4. **Authoritative referee** — wraps the C engine (or a Go port) for legality,
   checkmate/stalemate/insufficient-material, repetition, 50-move.
5. **Reconnect** — a room survives a seat disconnecting for N seconds; a holder
   of the seat token sends `hello{token}` and gets `state`.
6. **Observability** — structured logs, Prometheus metrics (rooms, connects,
   illegal moves), graceful shutdown.

### Room lifecycle
```
created ──join──▶ ready ──start──▶ playing ──gameover──▶ finished
   │                                     │
   └────────── abandoned ◀── timeout ────┘        (rematch → ready)
```

---

## 6. Client changes (SDL app)

### Transport abstraction
Introduce `NetTransport` vtable (`connect`, `send`, `poll`, `close`,
`state`) implemented by:
- `net_tcp.c` — current SDL2_net LAN transport (unchanged behavior),
- `net_ws.c` — WebSocket transport for online mode.
`local_tick`/`hostjoin_tick` are refactored to talk to the transport interface,
not to `Net` directly.

### New scenes / UI
- **Menu**: add **Online Multiplayer** (alongside Local Multiplayer).
- **`SCENE_ONLINE`** (replaces/enriches Host/Join for internet):
  - Sign-in / display name.
  - **Create room** → shows a 6-character code + copy button.
  - **Join room** → code entry field.
  - Connection status, cancellable wait, error messages.
- **In-game**: connection indicator, opponent presence ("opponent
  reconnecting…"), clock display, resign/draw/rematch buttons, chat panel.
- Reuse the existing auto-flip / SAN / status UI.

### State
- `MODE_ONLINE` (new `GameMode`) or reuse `MODE_LOCAL` with a `bool online`
  flag. Prefer a distinct mode so feature flags (clocks, resign) are explicit.
- Extend the in-memory `SavedGame` snapshot: online games are **not** resumable
  locally; resumption is server-side via the seat token.

---

## 7. Security & integrity

- **TLS everywhere** (`wss://`, `https://`). No plaintext online traffic.
- **Never trust the client**: server validates moves and computes results.
- **Auth**: anonymous signed token in v1 (device-scoped); optional OAuth later.
  Tokens are random 128-bit values; seat tokens are separate and single-use.
- **Rate limiting**: per-IP and per-session message caps; reject oversized
  frames; cap chat length.
- **Abuse**: profanity filtering / chat opt-out, report/block later; CSAM/abuse
  reporting policy before any public lobby.
- **Resource bounds**: max rooms per IP, idle-room reaper, per-connection
  buffers, timeouts.
- **Privacy**: minimal data (nick, result). Document retention; allow deletion.

---

## 8. Deployment & operations

- One small VM or PaaS (Fly.io / Render / Hetzner) + managed Postgres.
- Dockerfile; config via env (`PORT`, `DATABASE_URL`, `TLS_CERT`, `JWT_SECRET`).
- CI: build server, run unit + integration tests, publish image; deploy on tag.
- Health checks + autoscaling; sticky not required if rooms are single-process
  (use a consistent-hash router or Redis pub/sub before scaling out).
- Version negotiation: the server advertises a min/max protocol version; old
  clients get a clear "please update" message.

---

## 9. Testing strategy

- **Protocol unit tests** — frame encode/decode, versioning, unknown fields.
- **Referee tests** — reuse `tests/test_rules.c` cases server-side.
- **Integration harness** — spin up a local server and two headless clients
  (extend the approach in `tests/test_local.c`) to assert create/join/move/
  gameover/reconnect.
- **Chaos** — drop the socket mid-game, assert reconnect + `state` resync;
  duplicate/reordered/out-of-turn moves; illegal move injection.
- **Load** — many idle rooms; message flood; reconnect storms.
- **Security** — malformed frames, oversized payloads, token replay.

---

## 10. Milestones

| Phase | Deliverable | Notes |
| --- | --- | --- |
| **0. Refactor** | Transport vtable; LAN mode unchanged; protocol v2 types defined | No user-visible change; keeps `test_local` green |
| **1. Lobby** | Server with `create`/`join`/code + relay; client `SCENE_ONLINE` | Connect from two networks, no port forwarding |
| **2. Authoritative play** | Server validates moves, clocks, resign, gameover | Cheat-proof core |
| **3. Resilience** | Heartbeats, reconnect + `state` resync, rematch, presence | Survives brief drops |
| **4. Polish** | Draw offers, chat, spectators, sound/clock UI | |
| **5. Meta** | Accounts, ratings, matchmaking queue, history | Larger effort, optional |

Phases 0–2 are the minimum for a credible "play a friend online" release.
Phases 3–5 can ship incrementally.

---

## 11. Open questions

- Server language: **Go** vs **C** (engine reuse). *Decision needed before
  Phase 2.*
- Anonymous play vs required accounts for v1.
- Hosting budget / domain; TLS via Let's Encrypt or PaaS-managed.
- Whether to keep LAN TCP as the default and make Online opt-in, or unify.
- Clock time controls to expose (e.g. 5+0, 10+5, unlimited).

---

## 12. Immediate next step

Do **Phase 0** only: add the transport abstraction and freeze the v2 message
types, leaving LAN behavior and all existing tests untouched. That de-risks the
rest without committing to a server stack yet.
