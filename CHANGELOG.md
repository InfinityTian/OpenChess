# Changelog

All notable changes to OpenChess. The format follows
[Keep a Changelog](https://keepachangelog.com/), and the project uses
[Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- **Vector move-quality badges.** The badges are SVG-shaped (from the provided
  `chess_badges_svg` set plus Best/Excellent/Good) and loaded through SDL_image's
  SVG support, so they stay crisp at any size. The glyph outlines are generated
  with `scripts/gen_badge_glyphs.py` (nanosvg ignores `<text>`, which previously
  dropped `?!`, `??`, `?`, `!`, `!!`). Applied to the board, move list, coach line
  and review report. Excellent now uses a thumbs-up and Good a check.
- **Game report screen.** After a review a dedicated report shows **White vs
  Black** columns: player names, per-side **accuracy** and **average centipawn
  loss**, a per-class table (`count | badge | count`), and a Game Rating row.
  Open it with **V** or the **Report** button once a review exists.
- **Coach lines.** The analysis panel shows the **last move and its grade**
  (e.g. `Nd7 is best`, `Bd2 is a mistake`) above the engine's current best move.
- **Position-based opening explorer.** The Openings browser matches the **board
  position**, so different move orders that transpose count as the same opening.
  You can **play moves on the board** to walk the book, **flip** with **F**, and
  moves are animated.
- **Opening code.** Openings are shown as `ECO · Name` (e.g.
  `A00 · Amar Opening`) in Analysis and the browser.
- **Online/Offline switch.** A clickable pill on the welcome screen (and
  Settings → Gameplay → Mode) toggles offline mode, which disables the online
  menu entries and lets you create a **local profile** (name + ratings) from the
  Account screen without a server.
- The evaluation bar is **wider** with the numeric score drawn inside it, and it
  flips with the board.
- **Arcade animation rework.** Moves slide straight (no z-hop or magnify) with a
  tapered **comet trail** — blue for White, red for Black. A **check** shows a
  white corner-bracket halo around the king, and a **capture** shows a red ring
  plus a shrink/fade burst. Dragging hangs the piece below the cursor with a
  gentle size pulse and drops it with a fast slide; arrow-key tree navigation is
  animated (forward and reverse).
- Chess.com-style move indicators: a soft dot for quiet targets, a thick red
  ring for captures, and a radial glow behind the selected piece.
- The app icon has rounded corners (`scripts/round_icon.py`).

### Changed
- Importing a PGN now always **clears the previous analysis tree/board and the
  text box** before loading (no more merging into the old tree), and the
  uploaded PGN is shown **word-wrapped** in the import box. Review **no longer
  starts automatically** on upload — click **Analyze** / press **V**.
- The arcade **trail** is now a **thin, uniform-width** band with a fade and a
  **dynamic length** (grows from the start, holds about three squares, shrinks to
  nothing); blue for White, red for Black.
- The **capture indicator** is a true **ring** (no filled centre) with a
  **subtle glow**; the selection glow is likewise toned down.
- The factory appearance is **Icy Sea board + Cases pieces + Arcade animation**.
- The **check animation** now shows a near-opaque light mask, an expanding inner
  rectangle and corner brackets growing from the king to the square edges, then
  fading (matched to `check.mov`).
- **Review is decoupled from navigation**: clicking/arrowing through moves no
  longer interrupts a running review, and it no longer auto-opens the report.
  The button reads **Analyze** while reviewing and **Report** only when done.
- **Brilliant** is now checked before **Great**, and a sacrifice is recognised
  when the destination can be won (a cheaper attacker or net material loss), so
  moves like `21...Ng4` and `31...Rxf5` classify as `!!`.
- The welcome title reads **OpenChess**.

### Fixed
- Dragging a piece now keeps it centred on the cursor (no downward bias) with a
  clearer size pulse, plus a soft blue **halo** and a cyan highlight on the
  square under the cursor.
- The capture indicator is now a **high-resolution, anti-aliased ring** (built
  once into a texture) instead of a low-res raster ring.
- The GUI test suite no longer writes the user's real config
  (`OPENCHESS_CONFIG` is redirected to a temp file), which had been corrupting
  the saved board/piece selection; the default stays **Icy Sea + Cases + Arcade**.

### Added (earlier in this release)
- **Board move-quality badges.** In Analysis the played move's destination square
  is tinted and a colored circle badge is drawn on it: green star (best), blue
  `!` (great), teal `!!` (brilliant), amber `?!`, orange `?`, red `??`/`X`, and
  brown book.
- **Chess.com-style move tree panel.** The move list is now a scrollable table
  (move number, White, Black) with badges to the left of each move and variations
  inline on indented lines with a guide bar (e.g. `1. e4 e5 ( 1...c5 )`). The
  current move is highlighted and the panel auto-scrolls to follow play; the
  mouse wheel and click-drag scroll it.
- **Engine panel restyle.** A coach line shows the score chip plus
  `<best move> is best` (with a star) and the search depth; engine lines use
  score chips, and the opening name is shown for the current position.
- The vertical evaluation bar shows the numeric score and now flips with the
  board.

### Changed
- `!` is reserved for **Great** moves; **Best** no longer shows `"!"` (it uses a
  green star badge on the board instead).
- **Brilliant (`!!`)** now requires a genuine sacrifice: material is given up and
  the piece can actually be captured on its destination square, and the position
  stays good.
- **Promotion** shows the Q/R/B/N chooser on both drag and click moves, with the
  queen marked as the default; **double-clicking** the target square promotes to
  a queen directly.

## [2.0.0] - 2026-09-24

### Added
- **Accounts & ratings.** Optional SQLite-backed accounts on the server
  (`server/accounts.c`, PBKDF2-HMAC-SHA256 password hashing). Log in / register
  from the online lobby (**L**), log out (**O**); the session token is saved in
  `chess.conf` for auto-login. **PvP Elo** (opt-in **Rated** toggle, **R**) and a
  **puzzle Elo** are stored per account; puzzle results sync when logged in.
- **Puzzles mode.** A new welcome-menu entry trains Lichess puzzles with rating
  bands and themes (attacking/defending/endgame/…), including **Hint** and
  **Reveal** (marked incorrect) and a local puzzle Elo. `scripts/import_puzzles.py`
  builds a small local subset from the HF dataset
  (`Lichess/chess-puzzles-with-games`) or the official Lichess puzzle CSV, with
  theme-balanced sampling.
- **PGN import/export with variations.** A move-tree module (`src/movetree.*`) and
  a PGN parser/serializer (`pgn.c`) that handle `( … )` variations, `{}` comments,
  `$n` NAGs, `[%eval]` and headers.
- **Move review.** A chess.com-style classifier (`src/review.*`) for Brilliant,
  Great, Best, Excellent, Good, Book, Inaccuracy, Mistake, Blunder and Miss,
  plus an opening-book loader (`src/opening.*`) and
  `scripts/import_openings.sh` (lichess openings, CC0). In Analysis, press **V**
  to review the game — a Stockfish pass with a progress bar grades every move and
  shows its glyph in the move list.
- **PGN import.** **Ctrl+O** opens an import pop-up with a paste box (Ctrl+V) and
  an **Upload** button (loads a `.pgn` from the games folder); the loaded game is
  set up in Analysis and can be reviewed/exported (with variations).
- **Openings browser.** A new welcome entry lists the lichess opening book
  (CC0) with search; select a line, step it with Left/Right, and press Enter to
  load it into Analysis. `scripts/import_openings.sh` fetches `assets/openings.tsv`.
- **Account screen.** A visible **Account** menu entry (plus `L`/`O` in the
  online lobby) with local offline profile and puzzle rating, and online
  login/register with PvP + puzzle ratings synced from the server.
- **Move-tree merge.** Importing a second PGN that starts from the same position
  now **merges** it into the current analysis tree, so shared prefixes are reused
  and the new lines become sibling variations (`src/movetree.c`: `mt_merge`,
  `mt_copy`, `mt_find_child`).
- **Clickable move list + variations.** In Analysis, click any move (or a `var:`
  entry) to jump to that position, use Left/Right to step, and play after going
  back to create a new variation (the old line is kept).
- **Analysis import/analyse UI.** Visible **Import** and **Analyze** buttons;
  **Upload** uses a native file dialog (any path), and imported games are
  analysed automatically with a graded summary.
- Shared `uci_to_move()` in the rules engine (used by the server and puzzles).

### Changed
- The PGN move list now shows a full move per row (`1. e4 e5`).
- The welcome menu now shrinks its buttons adaptively so any number of entries
  fits without overlapping the footer.

### Fixed
- **Saved PGN could not be uploaded to chess.com.** SAN disambiguation ran for
  pawns too, producing invalid moves like `hhxg4` (should be `hxg4`); pawn
  captures are now disambiguated only by the origin file. The PGN serializer also
  emitted variations in the wrong place and wrote `!?`-style suffixes; it now
  places variations inline and uses numeric NAGs (`$1`…), and includes `Round`.
- **Threefold repetition and the fifty-move rule now count as draws** (client and
  server), with `1/2-1/2` results and clear reasons.
- **Puzzle rating** now updates dynamically per attempt: a wrong move counts the
  puzzle incorrect once (rating drops, ELO), solving it correctly earns a gain,
  and puzzles default to a window around your rating (**Around my rating**).
  Added an on-screen **Next** button, and the panel shows your rating and the
  change.
- Saved online games now export the authoritative result instead of `*`.

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
