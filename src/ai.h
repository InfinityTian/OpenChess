#ifndef AI_H
#define AI_H

#include "board.h"
#include "move.h"
#include <stdbool.h>

/*
 * Minimal UCI bridge to a chess engine (Stockfish by default). The engine runs
 * as a child process; searches happen there, so callers poll non-blockingly and
 * the UI never blocks while the engine thinks.
 */
typedef struct AiEngine AiEngine;

/*
 * Locate an engine binary. If `configured` is a readable/executable path it is
 * used; otherwise a `stockfish` binary is searched for in common locations and
 * on PATH. Returns a pointer to a static buffer, or NULL when not found.
 */
const char *ai_find_engine(const char *configured);

/*
 * Spawn the engine at `path` and perform the UCI handshake. Returns NULL on
 * any failure (engine missing, handshake timeout, ...).
 */
AiEngine *ai_start(const char *path);

/* Send "quit" and reap the child. Safe with NULL. */
void ai_stop(AiEngine *ai);

bool ai_alive(const AiEngine *ai);

void ai_new_game(AiEngine *ai);
void ai_set_skill(AiEngine *ai, int level);   /* 0..20 */
void ai_set_movetime(AiEngine *ai, int ms);   /* per-move think time */
void ai_set_depth(AiEngine *ai, int depth);   /* >0 overrides movetime */
void ai_set_threads(AiEngine *ai, int n);     /* CPU cores (1..) */
void ai_set_hash(AiEngine *ai, int mb);       /* transposition table MB */
void ai_set_multipv(AiEngine *ai, int n);     /* 1..AI_MAX_LINES */

/* Multi-PV analysis lines (populated from "info ... multipv K ... pv ..."). */
#define AI_MAX_LINES 5

typedef struct {
    int  cp;            /* centipawn score, side-to-move perspective */
    int  mate;          /* mate distance when has_mate */
    int  depth;
    int  multipv;       /* 1-based line number */
    bool has_mate;
    char pv[192];       /* space-separated UCI moves */
} AiLine;

/* Copy the up-to-`max` current lines and return how many were written. */
int ai_get_lines(const AiEngine *ai, AiLine *out, int max);

/* Begin searching the position `b`. */
void ai_go(AiEngine *ai, const Board *b);

/* Begin an unbounded search (used for live analysis; stop with ai_stop_search). */
void ai_go_infinite(AiEngine *ai, const Board *b);

/* Most recent "info" score, from the side-to-move's perspective. */
bool ai_get_eval(const AiEngine *ai, int *cp, int *mate, int *depth);

/* True when the most recent score was a "score mate N" line (N may be 0). */
bool ai_eval_has_mate(const AiEngine *ai);

/* Ask the engine to abort the current search (its pending bestmove is ignored). */
void ai_stop_search(AiEngine *ai);

/*
 * Non-blocking poll. Returns true and writes the UCI move (e.g. "e2e4",
 * "e7e8q") to `out_uci` once the engine reports "bestmove".
 */
bool ai_poll_bestmove(AiEngine *ai, char out_uci[8]);

/*
 * Resolve a UCI move string against the legal moves of `b`. Writes the matching
 * Move to *out and returns true, or false if there is no match.
 */
bool ai_uci_to_move(const Board *b, const char *uci, Move *out);

/* Encode a move as a UCI string ("e2e4", "e7e8q"). out must hold >= 6 bytes. */
void move_to_uci(Move m, char out[6]);

#endif
