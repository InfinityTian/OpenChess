# Prefer cc, but fall back to gcc (e.g. MSYS2/MinGW on Windows).
CC      = $(shell command -v cc 2>/dev/null || command -v gcc 2>/dev/null || echo cc)
CFLAGS  = -std=c11 -Wall -Wextra -O2 -g

# Windows (MSYS2/MinGW) executables carry a .exe suffix.
EXE :=
ifeq ($(OS),Windows_NT)
EXE := .exe
endif
# SDL_net (local multiplayer), SDL_mixer (sound) and libwebsockets (online
# multiplayer) are all optional.
NET_PC   := $(shell pkg-config --exists SDL2_net 2>/dev/null && echo SDL2_net)
MIX_PC   := $(shell pkg-config --exists SDL2_mixer 2>/dev/null && echo SDL2_mixer)
WS_PC    := $(shell pkg-config --exists libwebsockets 2>/dev/null && echo libwebsockets)
# SQLite backs the server's account store (optional).
SQLITE_PC := $(shell pkg-config --exists sqlite3 2>/dev/null && echo sqlite3)
HAVE_SDL_NET := $(if $(NET_PC),1,0)
HAVE_SDL_MIXER := $(if $(MIX_PC),1,0)
HAVE_WS := $(if $(WS_PC),1,0)
HAVE_ACCOUNTS := $(if $(SQLITE_PC),1,0)
SQLITE_CFLAGS := $(shell pkg-config --cflags $(SQLITE_PC) 2>/dev/null)
SQLITE_LIBS   := $(shell pkg-config --libs $(SQLITE_PC) 2>/dev/null)

# Homebrew's libwebsockets.pc does not propagate its OpenSSL dependency.
SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null)
SSL_LIBS   := $(shell pkg-config --libs openssl 2>/dev/null)
WS_CFLAGS  := $(shell pkg-config --cflags $(WS_PC) 2>/dev/null) $(SSL_CFLAGS)
WS_LIBS    := $(shell pkg-config --libs $(WS_PC) 2>/dev/null) $(SSL_LIBS)

SDL_PKGS   := sdl2 SDL2_ttf SDL2_image $(NET_PC) $(MIX_PC) $(WS_PC)
SDL_CFLAGS := $(shell pkg-config --cflags $(SDL_PKGS) 2>/dev/null) $(SSL_CFLAGS)
SDL_LIBS   := $(shell pkg-config --libs $(SDL_PKGS) 2>/dev/null) $(SSL_LIBS) -lm

CFLAGS += -DHAVE_SDL_NET=$(HAVE_SDL_NET) -DHAVE_SDL_MIXER=$(HAVE_SDL_MIXER) -DHAVE_WS=$(HAVE_WS) -DHAVE_ACCOUNTS=$(HAVE_ACCOUNTS)

VERSION ?= $(shell cat VERSION 2>/dev/null || echo 0.0.0)
CFLAGS  += -DOPENCHESS_VERSION=\"$(VERSION)\"

BIN      = openchess$(EXE)
SRC_CORE = src/board.c src/move.c src/pgn.c src/movetree.c src/review.c src/opening.c src/fen.c src/ai.c src/themes.c src/paths.c
OBJ_CORE = $(SRC_CORE:.c=.o)
# Transport abstraction + LAN (SDL2_net) and online (libwebsockets) backends.
NET_SRC  = src/transport.c src/net_tcp.c src/net_ws.c
# Online protocol vocabulary + vendored JSON, and the client session layer.
PROTO_SRC = src/proto.c src/cJSON.c
ONLINE_SRC = src/online.c
# Puzzle trainer data loader (uses cJSON).
PUZZLE_SRC = src/puzzle.c
# Audio links SDL_mixer, so it only builds into the SDL binaries (not the pure
# board/AI unit tests).
AUDIO_SRC = src/audio.c

# The online server binary is only built when libwebsockets is available.
SERVER = server/openchessd$(EXE)
ifeq ($(HAVE_WS),1)
SERVER_TARGET = $(SERVER)
TEST_ONLINE   = tests/test_online
else
SERVER_TARGET =
TEST_ONLINE   =
endif

all: $(BIN) $(SERVER_TARGET)

$(BIN): src/main.c src/gui.c $(NET_SRC) $(PROTO_SRC) $(ONLINE_SRC) $(PUZZLE_SRC) $(AUDIO_SRC) $(OBJ_CORE)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@ $(SDL_LIBS)

# The server compiles the shared rules engine so it can referee games.
SERVER_ENGINE = src/board.c src/move.c src/fen.c src/pgn.c src/movetree.c
SERVER_SRC    = server/main.c server/accounts.c src/proto.c src/cJSON.c $(SERVER_ENGINE)
$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) $(WS_CFLAGS) $(SQLITE_CFLAGS) $^ -o $@ $(WS_LIBS) $(SQLITE_LIBS) -lm

%.o: %.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

tests/test_gui: tests/test_gui.c src/gui.c $(NET_SRC) $(PROTO_SRC) $(ONLINE_SRC) $(PUZZLE_SRC) $(AUDIO_SRC) $(OBJ_CORE)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@$(EXE) $(SDL_LIBS)

tests/test_net: tests/test_net.c $(NET_SRC)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@$(EXE) $(SDL_LIBS)

tests/test_transport: tests/test_transport.c src/transport.c
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_proto: tests/test_proto.c src/proto.c src/cJSON.c
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_puzzle: tests/test_puzzle.c src/puzzle.c src/cJSON.c
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_review: tests/test_review.c src/review.c
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_opening: tests/test_opening.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_movetree: tests/test_movetree.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_online: tests/test_online.c src/net_ws.c src/transport.c src/proto.c src/cJSON.c
	$(CC) $(CFLAGS) $(WS_CFLAGS) $^ -o $@$(EXE) $(WS_LIBS)

tests/test_local: tests/test_local.c src/gui.c $(NET_SRC) $(PROTO_SRC) $(ONLINE_SRC) $(PUZZLE_SRC) $(AUDIO_SRC) $(OBJ_CORE)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $^ -o $@$(EXE) $(SDL_LIBS)

tests/test_rules: tests/test_rules.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_fen: tests/test_fen.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_pgn: tests/test_pgn.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

tests/test_ai: tests/test_ai.c $(OBJ_CORE)
	$(CC) $(CFLAGS) $^ -o $@$(EXE) -lm

test: tests/test_rules tests/test_fen tests/test_pgn tests/test_ai tests/test_transport tests/test_proto tests/test_puzzle tests/test_review tests/test_opening tests/test_movetree tests/test_net tests/test_local tests/test_gui $(TEST_ONLINE) $(SERVER_TARGET)
	./tests/test_rules$(EXE)
	./tests/test_fen$(EXE)
	./tests/test_pgn$(EXE)
	./tests/test_ai$(EXE)
	./tests/test_transport$(EXE)
	./tests/test_proto$(EXE)
	./tests/test_puzzle$(EXE)
	./tests/test_review$(EXE)
	./tests/test_opening$(EXE)
	./tests/test_movetree$(EXE)
	./tests/test_net$(EXE)
	./tests/test_local$(EXE)
ifeq ($(HAVE_WS),1)
	./tests/test_online$(EXE)
endif
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
	rm -f $(BIN) chess $(SERVER) tests/test_rules$(EXE) tests/test_fen$(EXE) tests/test_pgn$(EXE) tests/test_ai$(EXE) tests/test_transport$(EXE) tests/test_proto$(EXE) tests/test_puzzle$(EXE) tests/test_review$(EXE) tests/test_opening$(EXE) tests/test_movetree$(EXE) tests/test_net$(EXE) tests/test_local$(EXE) tests/test_online$(EXE) tests/test_gui$(EXE) gui_smoke.bmp $(OBJ_CORE)
	rm -rf dist

.PHONY: all test run clean install uninstall app dmg
