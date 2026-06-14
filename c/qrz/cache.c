#include "cache.h"
#include "../common/paths.h"

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct ham_cache {
    sqlite3 *db;
};

static int exec_simple(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "cache: %s\n", err ? err : "error");
        sqlite3_free(err);
    }
    return rc;
}

ham_cache *ham_cache_open(void) {
    if (ham_configdir_ensure() != 0) return NULL;

    char path[1024];
    ham_configfile(path, sizeof(path), "qrz.db");

    sqlite3 *db = NULL;
    if (sqlite3_open(path, &db) != SQLITE_OK) {
        fprintf(stderr, "cache: open %s: %s\n", path, sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }
    exec_simple(db, "PRAGMA journal_mode=WAL;");
    exec_simple(db,
        "CREATE TABLE IF NOT EXISTS qrz("
        "  call TEXT PRIMARY KEY,"
        "  data TEXT NOT NULL,"
        "  updated INTEGER NOT NULL"
        ");");

    ham_cache *c = calloc(1, sizeof(*c));
    c->db = db;
    return c;
}

void ham_cache_close(ham_cache *c) {
    if (!c) return;
    sqlite3_close(c->db);
    free(c);
}

int ham_cache_get(ham_cache *c, const char *call, ham_fields *out) {
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(c->db,
        "SELECT data FROM qrz WHERE call = ?", -1, &st, NULL);
    if (rc != SQLITE_OK) return -1;
    sqlite3_bind_text(st, 1, call, -1, SQLITE_TRANSIENT);
    int found = -1;
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *data = sqlite3_column_text(st, 0);
        if (data) {
            ham_fields_parse(out, (const char *)data);
            found = 0;
        }
    }
    sqlite3_finalize(st);
    return found;
}

int ham_cache_set(ham_cache *c, const char *call, const ham_fields *f) {
    char *data = ham_fields_serialize(f);
    if (!data) return -1;

    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(c->db,
        "INSERT OR REPLACE INTO qrz(call, data, updated) VALUES(?, ?, ?)",
        -1, &st, NULL);
    if (rc != SQLITE_OK) { free(data); return -1; }
    sqlite3_bind_text(st, 1, call, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, data, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 3, (sqlite3_int64)time(NULL));
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    free(data);
    return rc == SQLITE_DONE ? 0 : -1;
}

char **ham_cache_list_calls(ham_cache *c, size_t *n_out) {
    *n_out = 0;
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(c->db, "SELECT call FROM qrz ORDER BY call", -1, &st, NULL) != SQLITE_OK)
        return NULL;

    size_t cap = 32, n = 0;
    char **arr = malloc(cap * sizeof(char *));
    while (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *s = sqlite3_column_text(st, 0);
        if (!s) continue;
        if (n >= cap) { cap *= 2; arr = realloc(arr, cap * sizeof(char *)); }
        arr[n++] = strdup((const char *)s);
    }
    sqlite3_finalize(st);
    *n_out = n;
    return arr;
}
