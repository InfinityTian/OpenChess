#include "accounts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(HAVE_ACCOUNTS) && HAVE_ACCOUNTS
#include <sqlite3.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define PBKDF2_ITER 100000
#define TOKEN_LEN   32   /* bytes */

struct Accounts {
    sqlite3 *db;
};

bool accounts_available(void) { return true; }

static void to_hex(const unsigned char *in, int n, char *out)
{
    static const char *hx = "0123456789abcdef";
    for (int i = 0; i < n; i++) {
        out[i * 2]     = hx[in[i] >> 4];
        out[i * 2 + 1] = hx[in[i] & 0xf];
    }
    out[n * 2] = '\0';
}

static int from_hex(const char *in, unsigned char *out, int max)
{
    int n = 0;
    for (; in[0] && in[1] && n < max; in += 2, n++) {
        int hi = in[0] <= '9' ? in[0] - '0' : (in[0] | 0x20) - 'a' + 10;
        int lo = in[1] <= '9' ? in[1] - '0' : (in[1] | 0x20) - 'a' + 10;
        if (hi < 0 || hi > 15 || lo < 0 || lo > 15) return -1;
        out[n] = (unsigned char)((hi << 4) | lo);
    }
    return n;
}

static void make_hash(const char *pass, const char *salt_hex, char *out, size_t n)
{
    unsigned char salt[32];
    int sl = from_hex(salt_hex, salt, sizeof salt);
    unsigned char dk[32];
    PKCS5_PBKDF2_HMAC(pass, (int)strlen(pass), salt, sl, PBKDF2_ITER,
                      EVP_sha256(), (int)sizeof dk, dk);
    char hex[65];
    to_hex(dk, (int)sizeof dk, hex);
    snprintf(out, n, "%d$%s$%s", PBKDF2_ITER, salt_hex, hex);
}

static bool check_hash(const char *pass, const char *stored)
{
    int iter = 0;
    char salt_hex[65] = "", hash_hex[65] = "";
    if (sscanf(stored, "%d$%64[^$]$%64s", &iter, salt_hex, hash_hex) != 3)
        return false;
    if (iter <= 0) return false;

    unsigned char salt[32];
    int sl = from_hex(salt_hex, salt, sizeof salt);
    if (sl < 0) return false;
    unsigned char dk[32];
    PKCS5_PBKDF2_HMAC(pass, (int)strlen(pass), salt, sl, iter,
                      EVP_sha256(), (int)sizeof dk, dk);
    char hex[65];
    to_hex(dk, (int)sizeof dk, hex);

    /* constant-time-ish compare */
    unsigned diff = 0;
    for (int i = 0; hex[i] && hash_hex[i]; i++)
        diff |= (unsigned char)(hex[i] ^ hash_hex[i]);
    return diff == 0 && strlen(hex) == strlen(hash_hex);
}

static void new_token(char *out, size_t n)
{
    unsigned char buf[TOKEN_LEN];
    if (RAND_bytes(buf, sizeof buf) != 1) {
        for (size_t i = 0; i < sizeof buf; i++) buf[i] = (unsigned char)rand();
    }
    to_hex(buf, (int)sizeof buf, out);
    (void)n;
}

Accounts *accounts_open(const char *db_path)
{
    if (!db_path || !*db_path) return NULL;
    Accounts *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    if (sqlite3_open(db_path, &a->db) != SQLITE_OK) {
        sqlite3_close(a->db);
        free(a);
        return NULL;
    }
    const char *schema =
        "CREATE TABLE IF NOT EXISTS users("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " name TEXT UNIQUE NOT NULL,"
        " salt TEXT NOT NULL,"
        " hash TEXT NOT NULL,"
        " token TEXT,"
        " pvp_rating INTEGER NOT NULL DEFAULT 1500,"
        " puzzle_rating INTEGER NOT NULL DEFAULT 1500,"
        " created INTEGER);";
    if (sqlite3_exec(a->db, schema, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_close(a->db);
        free(a);
        return NULL;
    }
    return a;
}

void accounts_close(Accounts *a)
{
    if (!a) return;
    if (a->db) sqlite3_close(a->db);
    free(a);
}

int accounts_register(Accounts *a, const char *user, const char *pass, Account *out)
{
    if (!a || !a->db || !user || !*user || !pass || !*pass) return -1;
    if (strlen(user) >= sizeof out->name) return -1;

    unsigned char sb[16];
    if (RAND_bytes(sb, sizeof sb) != 1)
        for (size_t i = 0; i < sizeof sb; i++) sb[i] = (unsigned char)rand();
    char salt_hex[33];
    to_hex(sb, (int)sizeof sb, salt_hex);
    char hash[200];
    make_hash(pass, salt_hex, hash, sizeof hash);
    char token[80];
    new_token(token, sizeof token);

    sqlite3_stmt *st = NULL;
    const char *ins = "INSERT INTO users(name,salt,hash,token,pvp_rating,puzzle_rating,created)"
                      " VALUES(?,?,?,?,1500,1500,?)";
    if (sqlite3_prepare_v2(a->db, ins, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, user, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, salt_hex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 3, hash, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 4, token, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 5, (int)time(NULL));
    int rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) return 0;   /* unique violation -> taken */

    out->id = (int)sqlite3_last_insert_rowid(a->db);
    snprintf(out->name, sizeof out->name, "%s", user);
    snprintf(out->token, sizeof out->token, "%s", token);
    out->pvp_rating = 1500;
    out->puzzle_rating = 1500;
    return 1;
}

/* fetch a row by a WHERE column/value; returns 1/0/-1 */
static int fetch(Accounts *a, const char *where, const char *val, Account *out,
                 char *hash_out, size_t hash_n)
{
    char sql[160];
    snprintf(sql, sizeof sql,
             "SELECT id,name,token,pvp_rating,puzzle_rating,hash FROM users WHERE %s=?",
             where);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(a->db, sql, -1, &st, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, val, -1, SQLITE_TRANSIENT);
    int r = sqlite3_step(st);
    if (r == SQLITE_ROW) {
        out->id = sqlite3_column_int(st, 0);
        snprintf(out->name, sizeof out->name, "%s",
                 (const char *)sqlite3_column_text(st, 1));
        const unsigned char *tk = sqlite3_column_text(st, 2);
        snprintf(out->token, sizeof out->token, "%s", tk ? (const char *)tk : "");
        out->pvp_rating = sqlite3_column_int(st, 3);
        out->puzzle_rating = sqlite3_column_int(st, 4);
        if (hash_out) {
            const unsigned char *h = sqlite3_column_text(st, 5);
            snprintf(hash_out, hash_n, "%s", h ? (const char *)h : "");
        }
        sqlite3_finalize(st);
        return 1;
    }
    sqlite3_finalize(st);
    return 0;
}

int accounts_login(Accounts *a, const char *user, const char *pass, Account *out)
{
    if (!a || !a->db || !user || !pass) return -1;
    char hash[200];
    int r = fetch(a, "name", user, out, hash, sizeof hash);
    if (r != 1) return r;                  /* 0 unknown user */
    if (!check_hash(pass, hash)) return 0; /* wrong password */
    new_token(out->token, sizeof out->token);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(a->db, "UPDATE users SET token=? WHERE id=?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, out->token, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(st, 2, out->id);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
    return 1;
}

int accounts_login_token(Accounts *a, const char *token, Account *out)
{
    if (!a || !a->db || !token || !*token) return -1;
    return fetch(a, "token", token, out, NULL, 0);
}

void accounts_logout(Accounts *a, const char *token)
{
    if (!a || !a->db || !token) return;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(a->db, "UPDATE users SET token=NULL WHERE token=?", -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, token, -1, SQLITE_TRANSIENT);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

static void set_rating(Accounts *a, int user_id, const char *col, int rating)
{
    if (!a || !a->db) return;
    char sql[80];
    snprintf(sql, sizeof sql, "UPDATE users SET %s=? WHERE id=?", col);
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(a->db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, rating);
        sqlite3_bind_int(st, 2, user_id);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

void accounts_set_pvp(Accounts *a, int user_id, int rating)    { set_rating(a, user_id, "pvp_rating", rating); }
void accounts_set_puzzle(Accounts *a, int user_id, int rating) { set_rating(a, user_id, "puzzle_rating", rating); }

#else  /* !HAVE_ACCOUNTS */

bool accounts_available(void) { return false; }
Accounts *accounts_open(const char *p) { (void)p; return NULL; }
void accounts_close(Accounts *a) { (void)a; }
int accounts_register(Accounts *a, const char *u, const char *p, Account *o)
{ (void)a; (void)u; (void)p; (void)o; return -1; }
int accounts_login(Accounts *a, const char *u, const char *p, Account *o)
{ (void)a; (void)u; (void)p; (void)o; return -1; }
int accounts_login_token(Accounts *a, const char *t, Account *o)
{ (void)a; (void)t; (void)o; return -1; }
void accounts_logout(Accounts *a, const char *t) { (void)a; (void)t; }
void accounts_set_pvp(Accounts *a, int u, int r) { (void)a; (void)u; (void)r; }
void accounts_set_puzzle(Accounts *a, int u, int r) { (void)a; (void)u; (void)r; }

#endif
