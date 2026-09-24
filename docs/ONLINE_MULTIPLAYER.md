# Online Multiplayer — Design & Implementation Plan

Status: **Phases 0–3 complete, Phase 4 partly done** (draws, chat, spectators
and file-based result logging; accounts/ratings/DB deferred). The LAN **Local
Multiplayer** mode stays as-is; this document describes how OpenChess grows into
internet play.

> **Phase 0 (done):** `src/transport.h/.c` vtable, `src/net_tcp.c` (LAN) and
> `src/net_ws.c`, vendored cJSON, and the `src/proto.h/.c` message vocabulary.
> `libwebsockets` is detected as `HAVE_WS` with graceful fallback.
>
> **Phase 1 (done):** real libwebsockets client (`src/net_ws.c`), C server
> `server/openchessd` (sessions, rooms, matchmaking queue, relay), the
> `src/online.h/.c` session layer, the **Online Multiplayer** and **Online
> Matchmaking** menu entries + lobby screen, and the `test_online` integration
> test.
>
> **Phase 2 (done):** the server compiles the shared engine (`board/move/fen/pgn`)
> and is authoritative — it validates moves, sends `reject` + `state`, decides
> checkmate/stalemate/insufficient/fifty-move/timeout, and enforces an optional
> clock. The client applies the authoritative echo, resyncs on `state`, and shows
> clocks; the lobby has a time-control selector.
>
> **Phase 3 (done):** seat tokens + reconnect (a dropped player reclaims their
> seat and gets `state`), a disconnect grace period with `opponent` presence and
> "abandoned" timeout, heartbeats (server ping/client pong, client ping +
> auto-reconnect) and rematch (both agree; colours swap).
>
> **Phase 4 (partly done):** draw offers (offer/accept/decline → `1/2-1/2` on
> agreement), in-game chat relayed to the opponent and spectators, read-only
> **spectating** by room code, and finished-game logging to `$OPENCHESS_RESULTS`.
> Still planned: accounts + ratings, a rating-aware matchmaking queue, and
> database/HTTP history (libpq).

## Locked decisions

| Topic | Decision |
| --- | --- |
| Server language | **C** — reuse `src/board.c` / `src/move.c` / `src/fen.c` so there is a single rules engine |
| Client modes | **Two menu entries**: *Online Multiplayer* (private room code) and *Online Matchmaking* (public queue) |
| Transport | **WebSocket over TLS** (`wss://`) via **libwebsockets** (client *and* server share the same lib) |
| JSON | vendored **cJSON** (single file), keeps the build light |
| Persistence | optional **libpq** (Postgres) for results/ratings; v1 can run in-memory |
| Deployment | one small VM/PaaS Docker image; no P2P/NAT punching needed because clients only dial out |

---

## 1. Where we are today

- `src/net.c` / `src/net.h`: newline-delimited TCP on **SDL2_net**, non-blocking
  poll, one-line sends. LAN only.
- `src/gui.c`: `SCENE_HOSTJOIN` (IP + port), `enter_local_game`, `local_tick`.
- Protocol: `HELLO CHESS1 <white|black>`, `FEN <fen>`, `MOVE <uci>`, `BYE`.
- Missing for the internet: rendezvous/matchmaking, NAT-safe transport, TLS,
  reconnection, protocol versioning, server-side validation, clocks, rematch.

Chess is turn-based and tiny, so an **outbound-only relay** beats UDP hole
punching: it works through every NAT, CGNAT and firewall, and latency is
irrelevant.

---

## 2. User-facing modes (two entries)

Both appear in the welcome menu, next to the existing modes.

1. **Online Multiplayer** — private game. Create a room, share the 6-character
   code, the opponent joins that code. No account required.
2. **Online Matchmaking** — public game. Enter the queue and the server pairs
   you with the first waiting player (rating-aware later). No code to share.

Both converge on the same game session and `MODE_ONLINE` code path; only the
lobby step differs.

---

## 3. Architecture

```
┌──────────┐      wss:// (outbound only)      ┌─────────────────────────┐
│ Client A │ ────────────────────────────────▶│      Chess Server (C)   │
│  (SDL)   │◀──────────── relayed ────────────│  libwebsockets + cJSON  │
└──────────┘                                  │  ├ room manager         │
┌──────────┐                                   │  ├ matchmaking queue    │
│ Client B │ ────────────────────────────────▶│  ├ authoritative board  │
└──────────┘                                   │  └ (optional) libpq     │
                                              └─────────────────────────┘
```

- Clients make exactly one outbound `wss://` connection; nothing to configure.
- The server holds one `Board` per game. The **same C engine** used by the GUI is
  compiled into the server, so legality, checkmate, stalemate, repetition and the
  clocks are decided once, authoritatively.
- The server relays the accepted move to both seats and persists the result.

### Why libwebsockets for both sides
One dependency gives the client `wss://`, the server listener, and TLS via
OpenSSL/mbedTLS. It builds on macOS and Windows (MSYS2), matching the project's
existing cross-platform constraints.

---

## 4. Protocol (v2)

JSON object per WebSocket frame, `type` discriminator, `v` negotiated at
connect. Unknown fields ignored; unknown types no-op. LAN keeps the old line
protocol untouched.

### Client → Server
| type | fields | meaning |
| --- | --- | --- |
| `hello` | `v`, `token?`, `nick` | open/resume a session |
| `create` | `color?`, `clock?` | create a private room → returns code |
| `join` | `code` | join a private room by code |
| `queue` | `clock?` | enter public matchmaking |
| `cancel_queue` | — | leave matchmaking |
| `move` | `uci`, `ply` | propose a move |
| `resign` | — | resign |
| `draw_offer` / `draw_accept` / `draw_decline` | — | draw negotiation |
| `rematch` | — | request a new game in the same room |
| `chat` | `text` | rate-limited chat |
| `ping` | `t` | heartbeat / RTT |

### Server → Client
| type | fields | meaning |
| --- | --- | --- |
| `welcome` | `token`, `server_time` | session id (reconnect) |
| `queued` | `position?` | entered/updated matchmaking position |
| `room` | `code`, `color`, `white`, `black`, `clock` | room state |
| `start` | `fen`, `side`, `clock`, `opponent` | game begins |
| `move` | `uci`, `ply`, `san`, `clock`, `fen?` | accepted move (authoritative) |
| `reject` | `reason` | illegal/out-of-turn move |
| `state` | `fen`, `ply`, `clock`, `result?` | resync after reconnect |
| `gameover` | `result`, `reason` | checkmate/stalemate/resign/time/draw |
| `opponent` | `connected`, `nick` | presence |
| `chat` | `from`, `text` | relayed chat |
| `pong` | `t` | heartbeat reply |

Rules: server assigns sides, validates every move, computes clocks, and is the
only writer of results. Clients may pre-validate for instant feedback but accept
the server echo as truth.

---

## 5. Server (C) components

New `server/` directory, buildable on its own but linking the shared engine:

- `server/main.c` — config (env: `PORT`, `TLS_CERT`, `TLS_KEY`, `DATABASE_URL`),
  libwebsockets event loop, graceful shutdown.
- `server/session.c` — connection table, `hello`, tokens (128-bit random;
  separate single-use seat token), rate limits, bounds.
- `server/rooms.c` — `code → Room{seats, Board, Clock, tokens}`; lifecycle
  `created → ready → playing → finished`; abandoned-room reaper; `rematch`.
- `server/matchmaking.c` — FIFO (later rating-bucketed) queue of waiting
  sessions; pairs two, assigns White/Black, creates a room, emits `start`.
- `server/referee.c` — thin wrapper over `board/move/fen` + repetition/50-move;
  emits `move`/`reject`/`gameover`.
- `server/store.c` — optional libpq persistence of players/results (v1 can log
  only, add DB behind a flag).

HTTP endpoints on the same listener: `GET /healthz`, `GET /metrics`,
`POST /v1/session` (anonymous token). WebSocket upgrade at `/v1/ws`.

---

## 6. Client changes (SDL app)

### Transport abstraction (Phase 0)
- New `src/transport.h` vtable: `open/send/poll/role/state/close`.
- `src/net.c` → `src/net_tcp.c` (SDL2_net) implements it; `src/net.h` stays the
  LAN API.
- New `src/net_ws.c` implements it over libwebsockets (`wss://`). Build-guarded
  by `HAVE_WS`, mirroring the existing `HAVE_SDL_NET` fallback pattern.
- New `src/proto.c/.h` + vendored `src/cJSON.c/.h` for frame encode/decode.

### Scenes & modes
- `src/gui.h`: add `MODE_ONLINE` (`GameMode`) and `SCENE_ONLINE` (`Scene`); add
  online fields (`online_server`, `room_code`, `online_token`, `online_waiting`,
  `online_queueing`, clocks).
- Menu: add **Online Multiplayer** and **Online Matchmaking** entries; greyed out
  when `!HAVE_WS` (same mechanism as Local Multiplayer without SDL2_net).
- New handlers modelled on `SCENE_HOSTJOIN` (`gui.c:1725`): create/join room,
  show/copy code, matchmaking queue status; cancellable.
- New `online_tick()` beside `local_tick()` (`gui.c:~2730`) and
  `enter_online_game(g, color)` beside `enter_local_game` (`gui.c:1155`).
- In-game: connection indicator, opponent presence, clocks, **Resign**;
  `Undo`/`Restart` disabled (reuse the `MODE_LOCAL` guards at `gui.c:2251/2256`).
- `SavedGame`: skip `MODE_ONLINE` in `save_game` (online resume is server-side).

### Config
- Extend `gui_load_config`/`gui_save_config` and `src/paths.c` with
  `online_server`, `nick`, and a stored `online_token`.

---

## 7. Testing & CI

- **Protocol unit tests** — encode/decode, versioning, unknown fields
  (`tests/test_proto.c`).
- **Transport tests** — `tests/test_transport.c` plus the existing
  `tests/test_net.c` / `tests/test_local.c` stay green.
- **Referee tests** — reuse `tests/test_rules.c` cases server-side.
- **Integration harness** — start the C server locally and drive two headless
  clients (pattern from `tests/test_local.c`): create/join, matchmaking pairing,
  move/gameover, reconnect+resync, illegal-move injection.
- **Chaos/security** — socket drops, duplicate/reordered/out-of-turn moves,
  malformed/oversized frames, token replay, message floods.
- **CI** — build server + client, run all tests, publish the server Docker
  image; client DMG/app packaging (`scripts/make_app.sh`, `make dmg`) unchanged.

---

## 8. Milestones

| Phase | Deliverable | Notes |
| --- | --- | --- |
| **0. Refactor** ✅ | Transport vtable, vendored cJSON, `proto` types, `HAVE_WS` build plumbing | No user-visible change; LAN + all tests stay green |
| **1. Lobby + relay** ✅ | C server: sessions, rooms (code), matchmaking queue; client menu adds the **two entries** | Connect from two networks, no port forwarding |
| **2. Authoritative play** ✅ | Server referee/clocks/resign/gameover; client `reject`/`state` handling | Cheat-proof core |
| **3. Resilience** ✅ | Heartbeats, reconnect + resync, presence, rematch | Survives brief drops |
| **4. Meta** ◐ | Draw offers, chat, spectators, file result log ✅; accounts + ratings, rating queue, DB history remaining | Optional, incremental |

Phases 0–2 are the minimum for a shippable "play a friend online" release.

---

## 9. Security & operations

- TLS everywhere; never trust the client; server validates all moves.
- Anonymous signed tokens in v1; separate single-use seat tokens; rate limits;
  bounded frames; chat length caps; profanity/opt-out before public lobbies.
- Resource bounds: max rooms per IP, idle-room reaper, per-connection buffers.
- Dockerfile + env config; health checks; Prometheus metrics; consistent-hash
  router or Redis pub/sub before scaling beyond one process.
- Protocol min/max version advertised; old clients get "please update".

---

## 10. Risks

- New client dependency (libwebsockets/cJSON) on macOS + Windows → keep it
  optional behind `HAVE_WS`, with the same graceful-degradation pattern already
  used for SDL2_net/SDL2_mixer.
- Server ops/cost and abuse handling before a public matchmaking queue goes live.
- Client/server rules drift → prevented by compiling the shared C engine into
  both.

---

## 11. Immediate next step

**Phases 0–3 are done and Phase 4's social features are in.** What remains for a
full Phase 4: accounts + OAuth, Elo ratings persisted via **libpq**, a
rating-aware matchmaking queue, an HTTP `/v1/history` endpoint, and moderation
for chat/public lobbies. See `server/main.c` (rooms, draw/chat/spectate,
`log_result`), `src/online.c` and `src/gui.c`.
