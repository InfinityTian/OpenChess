# Prefer cc, but fall back to gcc (e.g. MSYS2/MinGW on Windows).
CC      = $(shell command -v cc 2>/dev/null || command -v gcc 2>/dev/null || echo cc)
CFLAGS  = -std=c11 -Wall -Wextra -O2 -g

# Windows (MSYS2/MinGW) executables carry a .exe suffix.
EXE :=
ifeq ($(OS),Windows_NT)
EXE := .exe
endif
# SDL_net (local multiplayer) and SDL_mixer (sound) are optional.
NET_PC   := $(shell pkg-config --exists SDL2_net 2>/dev/null && echo SDL2_net)
MIX_PC   := $(shell pkg-config --exists SDL2_mixer 2>/dev/null && echo SDL2_mixer)
HAVE_SDL_NET := $(if $(NET_PC),1,0)
HAVE_SDL_MIXER := $(if $(MIX_PC),1,0)

SDL_PKGS   := sdl2 SDL2_ttf SDL2_image $(NET_PC) $(MIX_PC)
SDL_CFLAGS := $(shell pkg-config --cflags $(SDL_PKGS) 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs $(SDL_PKGS) 2>/dev/null)

CFLAGS += -DHAVE_SDL_NET=$(HAVE_SDL_NET) -DHAVE_SDL_MIXER=$(HAVE_SDL_MIXER)

VERSION ?= $(shell cat VERSION 2>/dev/null || echo 0.0.0)
CFLAGS  += -DOPENCHESS_VERSION=\"$(VERSION)\"

BIN      = openchess$(EXE)
SRC_CORE = src/board.c src/move.c src/pgn.c src/fen.c src/ai.c src/themes.c src/paths.c
OBJ_CORE = $(SRC_CORE:.c=.o)
NET_SRC  = src/net.c
# Audio links SDL_mixer, so it only builds into the SDL binaries (not the pure
# board/AI unit tests).
AUDIO_SRC = src/audio.c

all: $(BIN)

$(BIN): src/main.c src/gui.c $(NET_SRC) $(AUDIO_SRC) $(OBJ_CORE)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@ $(SDL_LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

tests/test_gui: tests/test_gui.c src/gui.c $(NET_SRC) $(AUDIO_SRC) $(OBJ_CORE)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@$(EXE) $(SDL_LIBS)

tests/test_net: tests/test_net.c $(NET_SRC)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@$(EXE) $(SDL_LIBS)

tests/test_local: tests/test_local.c src/gui.c $(NET_SRC) $(AUDIO_SRC) $(OBJ_CORE)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@$(EXE) $(SDL_LIBS)

tests/test_rules: tests/test_rules.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE)

tests/test_fen: tests/test_fen.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE)

tests/test_pgn: tests/test_pgn.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE)

tests/test_ai: tests/test_ai.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE)

test: tests/test_rules tests/test_fen tests/test_pgn tests/test_ai tests/test_net tests/test_local tests/test_gui
	./tests/test_rules$(EXE)
	./tests/test_fen$(EXE)
	./tests/test_pgn$(EXE)
	./tests/test_ai$(EXE)
	./tests/test_net$(EXE)
	./tests/test_local$(EXE)
	./tests/test_gui$(EXE)
	@rm -f gui_smoke.bmp

run: $(BIN)
	./$(BIN)

# --- install / packaging --------------------------------------------------

PREFIX ?=
install: $(BIN)
	PREFIX="$(PREFIX)" ./install.sh

uninstall:
	PREFIX="$(PREFIX)" ./uninstall.sh

app: $(BIN)
	./scripts/make_app.sh

dmg: $(BIN)
	./scripts/make_dmg.sh

clean:
	rm -f $(BIN) chess tests/test_rules$(EXE) tests/test_fen$(EXE) tests/test_pgn$(EXE) tests/test_ai$(EXE) tests/test_net$(EXE) tests/test_local$(EXE) tests/test_gui$(EXE) gui_smoke.bmp $(OBJ_CORE)
	rm -rf dist

.PHONY: all test run clean install uninstall app dmg
