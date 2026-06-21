#include "fields.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/compat.h"

void ham_fields_init(ham_fields *f) {
    f->items = NULL; f->n = 0; f->cap = 0;
}

void ham_fields_free(ham_fields *f) {
    if (!f) return;
    for (size_t i = 0; i < f->n; i++) {
        free(f->items[i].key);
        free(f->items[i].val);
    }
    free(f->items);
    f->items = NULL; f->n = 0; f->cap = 0;
}

static void grow(ham_fields *f) {
    if (f->n < f->cap) return;
    f->cap = f->cap ? f->cap * 2 : 16;
    f->items = realloc(f->items, f->cap * sizeof(*f->items));
}

void ham_fields_set(ham_fields *f, const char *key, const char *val) {
    if (!key || !val) return;
    for (size_t i = 0; i < f->n; i++) {
        if (strcmp(f->items[i].key, key) == 0) {
            free(f->items[i].val);
            f->items[i].val = strdup(val);
            return;
        }
    }
    grow(f);
    f->items[f->n].key = strdup(key);
    f->items[f->n].val = strdup(val);
    f->n++;
}

const char *ham_fields_get(const ham_fields *f, const char *key) {
    if (!f || !key) return NULL;
    for (size_t i = 0; i < f->n; i++) {
        if (strcmp(f->items[i].key, key) == 0) return f->items[i].val;
    }
    return NULL;
}

/* Append n bytes to a growable heap buffer; returns 0 on success, -1 on OOM. */
static int buf_append(char **buf, size_t *len, size_t *cap,
                      const char *s, size_t n) {
    if (*len + n + 1 > *cap) {
        size_t ncap = *cap ? *cap : 256;
        while (*len + n + 1 > ncap) ncap *= 2;
        char *nb = realloc(*buf, ncap);
        if (!nb) return -1;
        *buf = nb; *cap = ncap;
    }
    memcpy(*buf + *len, s, n);
    *len += n;
    (*buf)[*len] = '\0';
    return 0;
}

/* Encode '\\' and '\n' inside values as "\\\\" and "\\n" so round-trip is safe. */
static int enc_append(char **buf, size_t *len, size_t *cap, const char *s) {
    for (; *s; s++) {
        if (*s == '\\') { if (buf_append(buf, len, cap, "\\\\", 2)) return -1; }
        else if (*s == '\n') { if (buf_append(buf, len, cap, "\\n", 2)) return -1; }
        else { if (buf_append(buf, len, cap, s, 1)) return -1; }
    }
    return 0;
}

/* open_memstream() is POSIX-only (absent on MinGW); build the buffer by hand
   so the output is byte-for-byte identical on every platform. */
char *ham_fields_serialize(const ham_fields *f) {
    char *buf = NULL; size_t len = 0, cap = 0;
    /* Guarantee a non-NULL, NUL-terminated result even for an empty set. */
    if (buf_append(&buf, &len, &cap, "", 0)) return NULL;
    for (size_t i = 0; i < f->n; i++) {
        if (buf_append(&buf, &len, &cap, f->items[i].key, strlen(f->items[i].key)) ||
            buf_append(&buf, &len, &cap, "=", 1) ||
            enc_append(&buf, &len, &cap, f->items[i].val) ||
            buf_append(&buf, &len, &cap, "\n", 1)) {
            free(buf);
            return NULL;
        }
    }
    return buf;
}

int ham_fields_parse(ham_fields *f, const char *s) {
    if (!s) return -1;
    const char *p = s;
    while (*p) {
        const char *eq = strchr(p, '=');
        if (!eq) break;
        size_t klen = eq - p;
        char *k = malloc(klen + 1);
        memcpy(k, p, klen); k[klen] = '\0';

        const char *q = eq + 1;
        char *v = malloc(strlen(q) + 1);
        size_t vi = 0;
        while (*q && *q != '\n') {
            if (*q == '\\' && q[1]) {
                q++;
                if (*q == 'n')      v[vi++] = '\n';
                else if (*q == '\\') v[vi++] = '\\';
                else                v[vi++] = *q;
                q++;
            } else {
                v[vi++] = *q++;
            }
        }
        v[vi] = '\0';
        ham_fields_set(f, k, v);
        free(k); free(v);
        if (*q == '\n') q++;
        p = q;
    }
    return 0;
}
