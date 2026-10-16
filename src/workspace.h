#ifndef WORKSPACE_H
#define WORKSPACE_H
#include "puzzle.h"
#include "cJSON.h"
#include <sqlite3.h>
#include <stdint.h>

typedef struct Workspace { sqlite3 *db; } Workspace;
Workspace *workspace_open(const char *file);
void workspace_close(Workspace *w);
/* UTC day number shifted to Beijing midnight. */
int workspace_today(void);
int workspace_day(const char *date);
void workspace_date(int day, char out[11]);
int workspace_weekday(int day); /* Monday = 0 */
int workspace_time_class(int base_ms, int inc_ms);
const char *workspace_class_name(int category);
bool workspace_daily_fill(Workspace *w, const Puzzles *p, int today);
bool workspace_daily(Workspace *w, int day, Puzzle *out);
cJSON *workspace_calendar(Workspace *w, const char *owner, int first, int last);
void workspace_streak(Workspace *w, const char *owner, int today, int *current, int *longest);
void workspace_profile(Workspace *w, const char *owner, int initial_rating);
int workspace_rating(Workspace *w, const char *owner, int category);
bool workspace_pick(Workspace *w, const Puzzles *p, const char *owner, int rating,
                    bool smart, const char *theme, Puzzle *out, char reason[128]);
/* Issuing and submitting the same attempt ID is idempotent. The stored puzzle
 * snapshot, not a claimed client rating/solution, controls every settlement. */
bool workspace_issue(Workspace *w, const char *owner, const char *id,
                     const Puzzle *p, bool rated, int daily_day);
cJSON *workspace_submit(Workspace *w, const char *owner, const char *id,
                         const char *moves, bool hint, bool reveal, int elapsed_ms);
sqlite3_stmt *workspace_stmt(Workspace *w, const char *sql);
cJSON *workspace_rows(sqlite3_stmt *s);
/* Returns heap-owned puzzle JSON; inverse accepts array or pipe-separated themes. */
cJSON *workspace_puzzle_json(const Puzzle *p);
bool workspace_puzzle_parse(const cJSON *j, Puzzle *out);
#endif
