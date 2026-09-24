# Changelog

All notable changes to OpenChess. The format follows
[Keep a Changelog](https://keepachangelog.com/), and the project uses
[Semantic Versioning](https://semver.org/).

## [1.4.0-beta1] - 2026-09-24

> Beta: online multiplayer. The protocol and server may still change before the
> final 1.4.0.

### Added
- **Online multiplayer (authoritative).** Two welcome-menu entries:
  **Online Multiplayer** (create/join a private room by 6-character code) and
  **Online Matchmaking** (public queue), talking to a C server over `ws(s)://`
  via **libwebsockets**.
- **Authoritative server** `server/openchessd` (built when libwebsockets is
  present). It compiles the shared rules engine and referees every game: it owns
  the board, validates each move, rejects illegal/out-of-turn moves with a
  `reject` + `state` resync, and declares **checkmate / stalemate / insufficient
  material / fifty-move / timeout**.
- **Clocks.** The server enforces an optional time control (`time`/`inc`) and
  includes `wtime`/`btime` on every `start`/`move`/`state`/`gameover`; the client
  shows interpolated clocks in the panel. The lobby has a **Time control**
  selector (Unlimited / 5+0 / 10+0 / 10+5 / 15+10).
- **Resilience.** Brief disconnects no longer end the game: seats are held for a
  **grace period** (default 30 s, `OPENCHESS_GRACE_MS`) while the opponent is told
  "opponent disconnected". A player who reconnects with their **session token**
  reclaims their seat and receives the authoritative `state`; if nobody returns
  in time the game is awarded to the opponent ("abandoned").
- **Heartbeats.** The server pings idle clients and watches for silence; the
  client pings too and auto-reconnects (up to 5 attempts) if the link drops
  mid-game, showing "Reconnecting ...".
- **Rematch.** After a game ends, the Restart button becomes **Rematch**; when
  both players agree the server swaps colours, resets the board/clocks and starts
  a new game.
- **Draw offers.** In an online game **Ctrl+D** offers a draw (or declines an
  incoming offer); **Ctrl+A** accepts. The server relays offers and ends the game
  `1/2-1/2` ("agreement") when accepted.
- **In-game chat.** Press **T** to type a line; messages are relayed to the
  opponent and any spectators and shown in the panel (rate-limited server-side).
- **Spectators.** A third person can watch a running game by room code — the
  lobby has a **Spectate** button; spectators see the live board/clocks and can
  chat, but cannot move.
- **Result logging.** Finished games are appended (one JSON object per line) to
  `$OPENCHESS_RESULTS` when set; no database required.
- **Client session layer** `src/online.h/.c` (authoritative move echo, `state`
  resync, clocks, heartbeat, reconnect) and a threaded WebSocket transport
  (`src/net_ws.c`) so the SDL loop never blocks on the network.
- Config keys `online_server` and `nick` (persisted in `chess.conf`).
- Tests: `test_online` starts the real server and covers create/join, illegal and
  out-of-turn rejection, both-seat move echo, fool's-mate checkmate, matchmaking,
  a clock flag, presence + reconnect (resume `state`), rematch colour swap,
  draw offer/accept, chat relay and spectating.

### Fixed
- **Network event loop.** libwebsockets' blocking `lws_service(ctx, timeout)` can
  stall in `poll()` on some builds (notably while reaping a dead peer). The client
  and server drive it non-blocking (`lws_service(ctx, 0)`) with their own pacing,
  and the server also runs a wakeup ticker (`lws_cancel_service`) so clocks and
  the disconnect grace period are enforced on schedule.

### Changed
- The welcome menu now lists eight entries; the GUI smoke test and LAN
  multiplayer keep working unchanged.

## [1.3.0] - 2026-09-24

### Added
- **Resume game.** Leaving a game with the **Menu** button stashes it in memory;
  the menu then offers a **Continue** entry (default-selected) that restores the
  board, move list, side, difficulty and engine state. Singleplayer and Analysis
  games are resumable; Local Multiplayer games are not, since the peer is gone.
- **Save PGN** button (formerly **PGN**) now renders with the small font so the
  longer label fits the same button.

### Changed
- **Setup screen** now marks the chosen side and difficulty at the same time;
  the keyboard focus is shown as a brighter border instead of hiding the other
  selection.
- **Annotations** erase when drawn twice: right-clicking the same square toggles
  it off (as before) and right-dragging the same arrow now removes it too.
- **PGN prompt** no longer prints the storage directory in its caption; the full
  saved path is still shown in the status message after saving.
- Shortcut legend under the move box uses a smaller font.

## [1.2.2] - 2026-09-22

### Added
- **Move sounds** using the chess.com *default* set: move (self/opponent),
  capture, castle, check, promotion, illegal and game-end, via optional
  **SDL2_mixer** (`scripts/import_sounds.sh` fetches `assets/sounds/`).
- **Settings panel** with three tabs: **Engine** (engine picker + Lines/Threads/
  Hash/Time/Depth), **Gameplay** (board, pieces, animation, Appearance picker)
  and **Audio & Video** (Sound, Max FPS). `Ctrl+E` opens it; choices are saved to
  `chess.conf`.
- **High framerate up to 320 fps**: `max_fps` (`0` = vsync, or 60/120/144/240/320)
  with a precise `SDL_GetPerformanceCounter` limiter and runtime vsync toggling.

### Fixed
- While dragging a piece, its origin square now shows the board texture instead
  of a black square.

## [1.2.1] - 2026-09-22

### Fixed
- **Castling notation.** SAN generation wrote `O-O`/`O-O-O` without advancing the
  write cursor, so a normal castle produced an *empty* move-list entry (and a
  checking castle just `+`/`#`). Castling now displays correctly in the move
  list, PGN export and engine lines.
- **Cursor drift after window zoom/maximize/resize.** Input now inverts the exact
  render transform the app installed (stored by `apply_render_scale`) instead of
  re-querying SDL mid-event, and that transform is re-applied on every resize —
  so clicking a square always matches what is drawn (previously clicks landed a
  square or more away after maximizing).

### Changed
- **Solid arrows.** Board arrows (user and engine) are drawn as one filled
  polygon; the shaft used to be parallel 1‑px lines that don't scale, which looked
  hollow/striped at non‑1× zoom and had a thin head.
- **Square highlights.** Right-click marks are now a translucent highlighted
  square with a border instead of a thin circle.

### Added
- **Engine arrows + toggle.** Each engine line's first move is drawn as a green
  arrow; an **Arrows** checkbox beside the Lines slider toggles it (persisted as
  `engine_arrows`, default on).

## [1.2.0] - 2026-09-22

### Added
- **Engine lines (MultiPV).** The analysis panel has a **0–5 slider**: `0` closes
  the engine, `1–5` shows that many engine lines, each with its score and the
  start of the principal variation in SAN.
- **Engine controls.** The Engine screen exposes *Lines*, *Threads* (CPU cores),
  *Hash*, *Time* and *Depth* (0 = unlimited), all persisted to `chess.conf`.
- **Board annotations.** Right-click toggles a circle, right-drag draws an arrow;
  hold **Ctrl** (green) or **Alt** (blue) to change the colour.
- **Native Windows support** (MSYS2 / MinGW-w64): process spawning, file paths and
  the build system now handle Windows; WSL2 instructions are included too.
- **Window/taskbar icon** taken from `assets/openchess.png`, matching the app and
  DMG artwork.

### Fixed
- **Board-resize grip drift.** Dragging the corner grip now zooms *and pans* in
  place (the window refits on release), so the grabbed point stays exactly under
  the cursor instead of drifting proportionally with the distance from the top.

### Changed
- Window title is now **OpenChess** (was `Chess`).
- `make` prefers `cc` and falls back to `gcc`; Windows builds emit `.exe`.

## [1.1.5] - 2026-09-22

### Fixed
- **Board-resize grip now tracks the cursor.** Dragging the bottom-right grip to
  magnify uses the grabbed point as a per-axis anchor (`board_x + 8·sq` /
  `board_y + 8·sq`) instead of `max(dx, dy) / (8·SQ_SIZE)`, so the handle no
  longer drifts away from the pointer (and zoom-out picks the correct axis).

### Added
- **App icon.** `packaging/OpenChess.png` is converted to `AppIcon.icns` and
  installed into the bundle (`CFBundleIconFile`), and reused as the DMG volume
  icon.
- **Drag-to-install DMG.** `make dmg` now stages an `Applications` shortcut and
  an arrow background, positions the icons with Finder, and builds an HFS+
  compressed image.

### Changed
- `make dmg` strips the quarantine attribute from the built `.app`/`.dmg` and
  prints the `sudo xattr -r -d com.apple.quarantine …` instructions for the
  receiving Mac (the app is unsigned/not notarized).

## [1.1.4] - 2026-09-21

### Fixed
- **Proportional click offset on scaled displays** (e.g. macOS "More Space"):
  input is now converted with an explicit inverse of the render transform
  (`viewport + base*scale`, with `output/window` as the event unit factor)
  instead of `SDL_RenderWindowToLogical`, which depends on SDL's `dpi_scale`
  and could be stale. The display density is also refreshed every frame.

### Added
- `OPENCHESS_DEBUG_UI=1` prints the mouse mapping (event, base, scale,
  viewport, output, window, zoom, ui_scale) on each click for troubleshooting.

## [1.1.3] - 2026-09-21

### Fixed
- **Large / fullscreen windows**: the UI canvas is now always scaled to fit the
  window and **centred** (letterboxed) instead of being pinned to the top-left
  with a capped zoom. The welcome menu stays on-screen and clicking an entry
  (e.g. the bottom **Quit**) works where it is drawn.
- Cleared the "board-driven resize" flag on mouse-up so a subsequent OS window
  resize is no longer ignored.

## [1.1.2] - 2026-09-21

### Fixed
- **Analysis evaluation could get stuck.** A race in the UCI state tracking
  meant a later re-analysis could skip the `stop` command, leaving Stockfish
  searching an old position (a near-constant +0.1 even on a checkmate). The
  engine is now synchronised (`stop` + `isready`/`readyok`) before each new
  position, so the board is always the one being evaluated.
- **Checkmate / mate scores** now show correctly on the evaluation bar and in
  the panel (`1-0` / `0-1` / `+M#`), instead of a small centipawn score.
- **Mouse bias when magnified.** Input now uses SDL's own
  `SDL_RenderWindowToLogical` conversion (matching the renderer's actual scale
  and DPI), the render scale is re-applied each frame, and display changes are
  handled — so clicks and drags land exactly where things are drawn.

### Added
- `OPENCHESS_DEBUG_UCI=1` logs the exact `position fen …` sent to the engine.

## [1.1.1] - 2026-09-21

### Fixed
- **Whole UI now magnifies when resizing.** The window is rendered on a fixed
  base canvas that is scaled uniformly, so the board, pieces, panel text,
  buttons and menus all grow/shrink together when the window is resized or the
  board grip is dragged. Previously only the board square size changed, leaving
  the text and controls at their original size.
- **Welcome menu layout.** Entries are now sized to the font and vertically
  centred in the space between the subtitle and the footer, so the buttons no
  longer overlap the title/subtitle or the message line.

## [1.1.0] - 2026-09-21

### Added
- **Resizable window and board.** Drag the grip in the board's bottom-right
  corner to resize the board; the window grows/shrinks to fit. The OS window is
  also resizable and the board scales to the available space. The chosen square
  size is saved as `board_size` in `chess.conf`.
- **Engine selection.** A new *Engine* screen (welcome menu or `Ctrl+E`) lists
  UCI engines found on `PATH` and accepts a custom executable path. The choice is
  saved to `chess.conf`.
- **Evaluation bar (Analysis).** A live engine evaluation bar runs down the left
  of the board and the score/depth is shown in the panel; the engine re-analyses
  after every move, undo, restart or FEN load.
- **Export game as PGN.** `Ctrl+S` or the **PGN** button saves the current game
  (headers + movetext + result) to
  `~/.local/share/openchess/games/<name>.pgn`.
- `--version` command-line flag; `VERSION` file.

### Changed
- Layout is computed at runtime instead of from fixed constants, enabling
  resizing; the side panel keeps a fixed width while the board scales.
- The side-panel button row now holds Undo / Restart / Styles / PGN / Menu.

## [1.0.0] - 2026-09-21

### Added
- Three modes: Analysis, Singleplayer vs Stockfish (UCI), Local Multiplayer
  over TCP (SDL2_net), with a welcome menu.
- Visual appearance picker (boards, pieces, animations) with thumbnails and all
  37 boards / 40 piece sets from `chess.com-boards-and-pieces`.
- FEN import/export and clipboard copy; board flipping.
- On-demand SAN input (press Enter to type a move).
- Bilingual (English / 简体中文) documentation.
- `install.sh` / `uninstall.sh` and macOS `.app`/`.dmg` packaging.

[1.2.2]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.2.2
[1.2.1]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.2.1
[1.2.0]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.2.0
[1.1.5]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.5
[1.1.4]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.4
[1.1.3]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.3
[1.1.2]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.2
[1.1.1]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.1
[1.1.0]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.0
[1.0.0]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.0.0
