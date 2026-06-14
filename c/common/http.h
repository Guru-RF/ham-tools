#ifndef HAM_HTTP_H
#define HAM_HTTP_H

#include <stddef.h>

typedef struct {
    char  *data;
    size_t size;
} ham_buf;

void ham_buf_free(ham_buf *b);

/* GET the URL. Returns 0 on success, -1 on error. user_agent may be NULL. */
int  ham_http_get(const char *url, const char *user_agent, ham_buf *out);

/* URL-encode a string. Caller frees. */
char *ham_url_encode(const char *s);

#endif
