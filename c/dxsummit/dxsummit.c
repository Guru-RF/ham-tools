#include "../common/config.h"
#include "../common/http.h"
#include "../common/paths.h"
#include "../common/spot.h"
#include "../common/spottui.h"

#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/compat.h"

/* ---------- HTML helpers ---------- */

static int ci_starts_with(const char *hay, const char *needle) {
    while (*needle) {
        if (!*hay) return 0;
        if (tolower((unsigned char)*hay) != tolower((unsigned char)*needle)) return 0;
        hay++; needle++;
    }
    return 1;
}

static void decode_entities_inplace(char *s) {
    char *r = s, *w = s;
    while (*r) {
        if (*r == '&') {
            if (strncmp(r, "&amp;", 5)  == 0) { *w++ = '&';  r += 5; continue; }
            if (strncmp(r, "&lt;",  4)  == 0) { *w++ = '<';  r += 4; continue; }
            if (strncmp(r, "&gt;",  4)  == 0) { *w++ = '>';  r += 4; continue; }
            if (strncmp(r, "&nbsp;",6)  == 0) { *w++ = ' ';  r += 6; continue; }
            if (strncmp(r, "&quot;",6)  == 0) { *w++ = '"';  r += 6; continue; }
            if (strncmp(r, "&#39;", 5)  == 0) { *w++ = '\''; r += 5; continue; }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

static char *html_to_text(const char *html) {
    size_t n = strlen(html);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t o = 0, i = 0;
    while (i < n) {
        if (html[i] == '<') {
            if (ci_starts_with(html + i, "<script")) {
                const char *end = strcasestr(html + i, "</script>");
                i = end ? (size_t)(end - html) + 9 : n;
                continue;
            }
            if (ci_starts_with(html + i, "<style")) {
                const char *end = strcasestr(html + i, "</style>");
                i = end ? (size_t)(end - html) + 8 : n;
                continue;
            }
            if (ci_starts_with(html + i, "<br")) {
                out[o++] = '\n';
                while (i < n && html[i] != '>') i++;
                if (i < n) i++;
                continue;
            }
            while (i < n && html[i] != '>') i++;
            if (i < n) i++;
            continue;
        }
        out[o++] = html[i++];
    }
    out[o] = '\0';
    decode_entities_inplace(out);
    return out;
}

static void normalize_ws(char *s) {
    char *r = s, *w = s;
    int last_space = 1;
    while (*r) {
        if (*r == '\r') { r++; continue; }
        if (*r == '\n') { *w++ = '\n'; last_space = 1; r++; continue; }
        if (isspace((unsigned char)*r)) {
            if (!last_space) *w++ = ' ';
            last_space = 1; r++;
        } else {
            *w++ = *r++;
            last_space = 0;
        }
    }
    *w = '\0';
}

/* ---------- spot line parser ---------- */

static void copy_token(char *dst, size_t dsz, const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
    size_t i = 0;
    while (**p && !isspace((unsigned char)**p) && i + 1 < dsz) dst[i++] = *(*p)++;
    dst[i] = '\0';
}

static int is_month_abbrev(const char *s) {
    static const char *m[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec", NULL
    };
    for (int i = 0; m[i]; i++) if (strncasecmp(s, m[i], 3) == 0) return 1;
    return 0;
}

static const char *find_timestamp(const char *p, const char *end) {
    for (const char *q = end - 9; q >= p; q--) {
        if (!isdigit((unsigned char)q[0]) || !isdigit((unsigned char)q[1]) ||
            !isdigit((unsigned char)q[2]) || !isdigit((unsigned char)q[3])) continue;
        if (q[4] != ' ') continue;
        const char *d = q + 5;
        if (!isdigit((unsigned char)d[0])) continue;
        int dlen = isdigit((unsigned char)d[1]) ? 2 : 1;
        if (d[dlen] != ' ') continue;
        const char *m = d + dlen + 1;
        if (m + 3 > end) continue;
        if (!is_month_abbrev(m)) continue;
        if (q > p && !isspace((unsigned char)q[-1])) continue;
        return q;
    }
    return NULL;
}

static int parse_spot_line(const char *line, ham_spot *s) {
    while (*line && isspace((unsigned char)*line)) line++;
    if (!*line) return -1;

    const char *p = line;
    copy_token(s->spotter, sizeof(s->spotter), &p);
    copy_token(s->freq,    sizeof(s->freq),    &p);
    copy_token(s->dx,      sizeof(s->dx),      &p);
    if (!*s->spotter || !*s->freq || !*s->dx) return -1;

    while (*p && isspace((unsigned char)*p)) p++;

    const char *end = p + strlen(p);
    const char *ts  = find_timestamp(p, end);

    if (ts) {
        size_t info_len = (size_t)(ts - p);
        while (info_len > 0 && isspace((unsigned char)p[info_len - 1])) info_len--;
        if (info_len >= sizeof(s->info)) info_len = sizeof(s->info) - 1;
        memcpy(s->info, p, info_len);
        s->info[info_len] = '\0';

        memcpy(s->timez, ts, 4);
        s->timez[4] = '\0';

        const char *cc = ts + 5;
        while (*cc && *cc != ' ') cc++;
        while (*cc == ' ') cc++;
        cc += 3;
        while (*cc && isspace((unsigned char)*cc)) cc++;
        strncpy(s->country, cc, sizeof(s->country) - 1);
        s->country[sizeof(s->country) - 1] = '\0';

        size_t cl = strlen(s->country);
        while (cl > 0 && isspace((unsigned char)s->country[cl - 1])) s->country[--cl] = '\0';
    } else {
        strncpy(s->info, p, sizeof(s->info) - 1);
        s->info[sizeof(s->info) - 1] = '\0';
        s->timez[0]   = '\0';
        s->country[0] = '\0';
    }
    s->mode[0] = '\0';
    return 0;
}

/* ---------- producer ---------- */

static void *fetch_thread(void *arg) {
    (void)arg;
    const char *url =
        "http://www.dxsummit.fi/DxSpots.aspx?count=50&include_modes=PHONE";
    while (ham_tui_running()) {
        ham_buf body = {0};
        if (ham_http_get(url, "ham-tools-dxsummit-c", &body) == 0) {
            char *text = html_to_text(body.data);
            if (text) {
                normalize_ws(text);
                ham_spot arr[HAM_SPOTTUI_MAX];
                int n = 0;
                char *save = NULL;
                for (char *line = strtok_r(text, "\n", &save);
                     line && n < HAM_SPOTTUI_MAX;
                     line = strtok_r(NULL, "\n", &save)) {
                    ham_spot s; memset(&s, 0, sizeof(s));
                    if (parse_spot_line(line, &s) == 0) arr[n++] = s;
                }
                ham_tui_set_spots(arr, n);
                free(text);
            }
        }
        ham_buf_free(&body);
        for (int i = 0; i < 15 && ham_tui_running(); i++) sleep(1);
    }
    return NULL;
}

/* ---------- dump mode ---------- */

static int run_dump(void) {
    const char *url =
        "http://www.dxsummit.fi/DxSpots.aspx?count=50&include_modes=PHONE";
    ham_buf body = {0};
    if (ham_http_get(url, "ham-tools-dxsummit-c", &body) != 0) {
        fprintf(stderr, "fetch failed\n"); return 1;
    }
    char *text = html_to_text(body.data);
    ham_buf_free(&body);
    if (!text) return 1;
    normalize_ws(text);

    char *save = NULL;
    int n = 0;
    for (char *line = strtok_r(text, "\n", &save);
         line; line = strtok_r(NULL, "\n", &save)) {
        ham_spot s; memset(&s, 0, sizeof(s));
        if (parse_spot_line(line, &s) != 0) continue;
        printf("%-10s | %-8s | %-12s | %-35s | %-4s | %s\n",
               s.spotter, s.freq, s.dx, s.info, s.timez, s.country);
        n++;
    }
    free(text);
    fprintf(stderr, "parsed %d spots\n", n);
    return 0;
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--dump") == 0) return run_dump();

    ham_config cfg;
    int cfg_rc = ham_config_load(&cfg);
    if (cfg_rc == HAM_CONFIG_CREATED) return 0;
    if (cfg_rc != 0) return 1;

    ham_tui_init("Country", cfg.fifo_path);

    pthread_t tid;
    pthread_create(&tid, NULL, fetch_thread, NULL);

    ham_tui_run();
    ham_tui_request_exit();
    pthread_join(tid, NULL);
    ham_tui_cleanup();
    ham_config_free(&cfg);
    return 0;
}
