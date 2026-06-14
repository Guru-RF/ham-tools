#ifndef HAM_CACHE_H
#define HAM_CACHE_H

#include "fields.h"

typedef struct ham_cache ham_cache;

ham_cache *ham_cache_open(void);
void       ham_cache_close(ham_cache *c);

/* Returns 0 if found (fills out fields), -1 if not found. */
int  ham_cache_get(ham_cache *c, const char *call, ham_fields *out);
int  ham_cache_set(ham_cache *c, const char *call, const ham_fields *f);

/* Returns malloc'ed array of strdup'd call strings; *n is count. Caller frees. */
char **ham_cache_list_calls(ham_cache *c, size_t *n);

#endif
