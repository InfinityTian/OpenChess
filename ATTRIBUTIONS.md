# Attributions / 致谢与素材来源

## English

### Chess boards and pieces

The images under `assets/boards/` and `assets/pieces/` were collected from the
third-party repository
[GiorgioMegrelli/chess.com-boards-and-pieces](https://github.com/GiorgioMegrelli/chess.com-boards-and-pieces),
which in turn downloads them from **chess.com**. That repository does not declare
a license, and the artwork is the property of Chess.com / its licensors.

They are bundled here for personal and educational use. Refer to chess.com's
terms before redistributing them or using them commercially. If you are a rights
holder and want an asset removed, please open an issue.

Labels in `assets/themes.txt` are derived from the same repository's
`default-config.json`.

### Sounds

The audio files under `assets/sounds/` are the chess.com **default** sound theme,
downloaded from
`https://images.chesscomfiles.com/chess-themes/sounds/_MP3_/default.zip`
(see `scripts/import_sounds.sh`). The sounds are the property of Chess.com and are
bundled here for personal and educational use only.

### Move-quality badges

The badges under `assets/badges/` are derived from the `chess_badges_svg` set
provided for this project (book, `??`, `?`, `?!`, `!?`, `!`, `!!`, X); the Best,
Excellent and Good badges were authored to match the same style. The `?` and `!`
glyph outlines were generated from **Arial Bold** with
`scripts/gen_badge_glyphs.py` (fontTools) so they render under nanosvg, which
ignores SVG `<text>`. They are bundled for use in the Analysis board, move list
and game report.

### Openings data

`assets/openings.tsv` is derived from the **lichess-org/chess-openings** dataset
(`https://github.com/lichess-org/chess-openings`), released under **CC0 1.0**
(public domain). It is fetched by `scripts/import_openings.sh` and used by the
Openings browser and the review's "book" classification.

### Fonts

OpenChess does not bundle fonts; it uses a system font (Arial Unicode on macOS,
DejaVu Sans on Linux) for glyphs and UI text.

### Libraries

- **SDL2**, **SDL2_ttf**, **SDL2_image**, **SDL2_net**, **SDL2_mixer** — zlib license.
- **cJSON** (vendored under `src/cJSON.c`, `src/cJSON.h`, v1.7.18) — MIT license.
- **libwebsockets** — optional, for online multiplayer (MIT license); not bundled.
- **Stockfish** — GPLv3. It is an external program that OpenChess launches; it is
  not distributed with this project.

Source code is licensed under the MIT License — see [`LICENSE`](LICENSE).

## 简体中文

### 棋盘与棋子

`assets/boards/` 与 `assets/pieces/` 下的图片来自第三方仓库
[GiorgioMegrelli/chess.com-boards-and-pieces](https://github.com/GiorgioMegrelli/chess.com-boards-and-pieces)，
该仓库从 **chess.com** 下载这些素材。该仓库未声明许可证，素材版权归 Chess.com
及其授权方所有。

本仓库仅为个人学习与教育用途打包这些素材。再分发或商用前请查阅 chess.com 的
条款。若您是权利方并希望移除某些素材，请提交 issue。

`assets/themes.txt` 中的名称来自同一仓库的 `default-config.json`。

### 音效

`assets/sounds/` 下的音效来自 chess.com 的 **default** 音效主题，下载自
`https://images.chesscomfiles.com/chess-themes/sounds/_MP3_/default.zip`
（见 `scripts/import_sounds.sh`）。音效版权归 Chess.com 所有，此处仅为个人学习
与教育用途打包。

### 走法质量徽章

`assets/badges/` 下的徽章来自为本项目提供的 `chess_badges_svg` 素材集（book、
`??`、`?`、`?!`、`!?`、`!`、`!!`、X）；Best、Excellent、Good 三个徽章为匹配同一
风格自行绘制。用于分析棋盘、着法列表与对局报告。

### 开局数据

`assets/openings.tsv` 来自 **lichess-org/chess-openings** 数据集
（`https://github.com/lichess-org/chess-openings`），采用 **CC0 1.0**（公共领域）
许可。该文件由 `scripts/import_openings.sh` 获取，用于开局浏览器与复盘时的
“book（开局库）”判定。

### 字体

OpenChess 不打包字体，仅使用系统字体（macOS 为 Arial Unicode，Linux 为
DejaVu Sans）渲染字形与界面文字。

### 依赖库

- **SDL2**、**SDL2_ttf**、**SDL2_image**、**SDL2_net**、**SDL2_mixer** —— zlib 许可证。
- **cJSON**（内置于 `src/cJSON.c`、`src/cJSON.h`，v1.7.18）—— MIT 许可证。
- **libwebsockets** —— 可选，用于在线对战（MIT 许可证）；本项目不打包。
- **Stockfish** —— GPLv3。它是由 OpenChess 启动的外部程序，本项目不附带分发。

源代码采用 MIT 许可证 —— 详见 [`LICENSE`](LICENSE)。
