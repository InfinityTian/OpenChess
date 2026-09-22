# Changelog

All notable changes to OpenChess. The format follows
[Keep a Changelog](https://keepachangelog.com/), and the project uses
[Semantic Versioning](https://semver.org/).

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

[1.1.3]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.3
[1.1.2]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.2
[1.1.1]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.1
[1.1.0]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.1.0
[1.0.0]: https://github.com/InfinityTian/OpenChess/releases/tag/v1.0.0
