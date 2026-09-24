#ifndef ACCOUNTS_H
#define ACCOUNTS_H

#include <stdbool.h>

/*
 * Account store for online play: username, PBKDF2 password hash, session token
 * and PvP/puzzle ratings. Backed by SQLite when built with HAVE_ACCOUNTS; all
 * calls degrade to "unavailable" otherwise.
 */

typedef struct Accounts Accounts;

typedef struct {
    int  id;
    char name[40];
    char token[80];
    int  pvp_rating;
    int  puzzle_rating;
} Account;

Accounts *accounts_open(const char *db_path);
void      accounts_close(Accounts *a);
bool      accounts_available(void);

/* 1 = success, 0 = name taken / bad credentials, -1 = error/store unavailable. */
int  accounts_register(Accounts *a, const char *user, const char *pass, Account *out);
int  accounts_login(Accounts *a, const char *user, const char *pass, Account *out);
int  accounts_login_token(Accounts *a, const char *token, Account *out);
void accounts_logout(Accounts *a, const char *token);

void accounts_set_pvp(Accounts *a, int user_id, int rating);
void accounts_set_puzzle(Accounts *a, int user_id, int rating);

#endif
