#include "http.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ham_buf_free(ham_buf *b) {
    if (!b) return;
    free(b->data);
    b->data = NULL;
    b->size = 0;
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    ham_buf *b = userdata;
    size_t n = size * nmemb;
    char *p = realloc(b->data, b->size + n + 1);
    if (!p) return 0;
    b->data = p;
    memcpy(b->data + b->size, ptr, n);
    b->size += n;
    b->data[b->size] = '\0';
    return n;
}

int ham_http_get(const char *url, const char *user_agent, ham_buf *out) {
    out->data = NULL;
    out->size = 0;

    CURL *c = curl_easy_init();
    if (!c) return -1;

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, out);
    if (user_agent) curl_easy_setopt(c, CURLOPT_USERAGENT, user_agent);

    CURLcode rc = curl_easy_perform(c);
    long http = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(c);

    if (rc != CURLE_OK || http < 200 || http >= 400) {
        ham_buf_free(out);
        return -1;
    }
    return 0;
}

char *ham_url_encode(const char *s) {
    CURL *c = curl_easy_init();
    if (!c) return NULL;
    char *enc = curl_easy_escape(c, s, 0);
    char *copy = enc ? strdup(enc) : NULL;
    if (enc) curl_free(enc);
    curl_easy_cleanup(c);
    return copy;
}
