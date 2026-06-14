#include "fields.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Encode \n and = inside values as \\n and \\e so round-trip is safe. */
static void enc_write(FILE *fp, const char *s) {
    for (; *s; s++) {
        if (*s == '\\')      fputs("\\\\", fp);
        else if (*s == '\n') fputs("\\n", fp);
        else                 fputc(*s, fp);
    }
}

char *ham_fields_serialize(const ham_fields *f) {
    char *buf = NULL; size_t len = 0;
    FILE *fp = open_memstream(&buf, &len);
    if (!fp) return NULL;
    for (size_t i = 0; i < f->n; i++) {
        fputs(f->items[i].key, fp);
        fputc('=', fp);
        enc_write(fp, f->items[i].val);
        fputc('\n', fp);
    }
    fclose(fp);
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
