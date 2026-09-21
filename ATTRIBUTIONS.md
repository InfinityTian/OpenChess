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

### Fonts

OpenChess does not bundle fonts; it uses a system font (Arial Unicode on macOS,
DejaVu Sans on Linux) for glyphs and UI text.

### Libraries

- **SDL2**, **SDL2_ttf**, **SDL2_image**, **SDL2_net** — zlib license.
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

### 字体

OpenChess 不打包字体，仅使用系统字体（macOS 为 Arial Unicode，Linux 为
DejaVu Sans）渲染字形与界面文字。

### 依赖库

- **SDL2**、**SDL2_ttf**、**SDL2_image**、**SDL2_net** —— zlib 许可证。
- **Stockfish** —— GPLv3。它是由 OpenChess 启动的外部程序，本项目不附带分发。

源代码采用 MIT 许可证 —— 详见 [`LICENSE`](LICENSE)。
