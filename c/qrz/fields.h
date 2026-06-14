#ifndef HAM_FIELDS_H
#define HAM_FIELDS_H

#include <stddef.h>

typedef struct {
    char *key;
    char *val;
} ham_field;

typedef struct {
    ham_field *items;
    size_t     n;
    size_t     cap;
} ham_fields;

void        ham_fields_init(ham_fields *f);
void        ham_fields_free(ham_fields *f);
void        ham_fields_set(ham_fields *f, const char *key, const char *val);
const char *ham_fields_get(const ham_fields *f, const char *key);

/* Serialize as "key=value\n..." into malloc()ed string (caller frees). */
char *ham_fields_serialize(const ham_fields *f);
/* Parse same format. */
int   ham_fields_parse(ham_fields *f, const char *s);

#endif
