#include "../common/config.h"
#include "../common/spot.h"
#include "../common/spottui.h"

#include <jansson.h>
#include <libwebsockets.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../common/compat.h"

/* ---------- shared state ---------- */

static struct lws_context *g_ctx;
static struct lws         *g_wsi;
static volatile int        g_initial_sent = 0;
static volatile int        g_want_initial = 1;
static double              g_last_time    = 0.0;

/* Accumulator for message fragments */
static char   *g_msg_buf;
static size_t  g_msg_len;
static size_t  g_msg_cap;

static int g_dump_mode = 0;

/* ---------- JSON → ham_spot ---------- */

static void fmt_time(double unix_ts, char *out, size_t n) {
    time_t t = (time_t)unix_ts;
    struct tm tm;
    gmtime_r(&t, &tm);
    snprintf(out, n, "%02d%02d", tm.tm_hour, tm.tm_min);
}

static const char *js_str(json_t *obj, const char *k) {
    json_t *v = json_object_get(obj, k);
    if (!v || !json_is_string(v)) return "";
    return json_string_value(v);
}

static int spot_from_json(json_t *obj, ham_spot *s) {
    memset(s, 0, sizeof(*s));

    const char *spotter = js_str(obj, "spotter_callsign");
    const char *dx      = js_str(obj, "dx_callsign");
    const char *mode    = js_str(obj, "mode");
    const char *country = js_str(obj, "dx_country");
    const char *comment = js_str(obj, "comment");

    strncpy(s->spotter, spotter, sizeof(s->spotter) - 1);
    strncpy(s->dx,      dx,      sizeof(s->dx) - 1);
    strncpy(s->mode,    mode,    sizeof(s->mode) - 1);
    strncpy(s->country, country, sizeof(s->country) - 1);
    strncpy(s->info,    comment, sizeof(s->info) - 1);

    json_t *f = json_object_get(obj, "freq");
    double freq_khz = 0.0;
    if (f && json_is_number(f)) freq_khz = json_number_value(f);
    snprintf(s->freq, sizeof(s->freq), "%.1f", freq_khz);

    json_t *t = json_object_get(obj, "time");
    if (t && json_is_number(t)) fmt_time(json_number_value(t), s->timez, sizeof(s->timez));

    if (!*s->spotter || !*s->dx) return -1;
    return 0;
}

/* ---------- JSON payload handler ---------- */

static void handle_payload(const char *data, size_t len) {
    json_error_t jerr;
    json_t *root = json_loadb(data, len, 0, &jerr);
    if (!root) {
        fprintf(stderr, "holycluster: bad JSON: %s\n", jerr.text);
        return;
    }

    json_t *spots = json_object_get(root, "spots");
    if (!spots || !json_is_array(spots)) {
        json_decref(root);
        return;
    }

    const char *type = js_str(root, "type");
    int is_initial = (strcasecmp(type, "initial") == 0) || !g_initial_sent;

    size_t n = json_array_size(spots);
    if (n == 0) { json_decref(root); return; }

    if (is_initial || g_dump_mode) {
        ham_spot arr[HAM_SPOTTUI_MAX];
        int k = 0;
        for (size_t i = 0; i < n && k < HAM_SPOTTUI_MAX; i++) {
            ham_spot s;
            if (spot_from_json(json_array_get(spots, i), &s) == 0) arr[k++] = s;
        }
        if (g_dump_mode) {
            for (int i = 0; i < k; i++) {
                ham_spot *s = &arr[i];
                printf("%-10s | %-8s | %-12s | %-8s | %-30s | %-4s | %s\n",
                       s->spotter, s->freq, s->dx, s->mode, s->info, s->timez, s->country);
            }
            fflush(stdout);
            fprintf(stderr, "parsed %d spots\n", k);
        } else {
            ham_tui_set_spots(arr, k);
        }
    } else {
        /* Incremental update — prepend each new spot. */
        for (size_t i = 0; i < n; i++) {
            ham_spot s;
            if (spot_from_json(json_array_get(spots, i), &s) == 0) ham_tui_add_spot(&s);
        }
    }

    /* Track latest timestamp for resume-after-reconnect. */
    for (size_t i = 0; i < n; i++) {
        json_t *tv = json_object_get(json_array_get(spots, i), "time");
        if (tv && json_is_number(tv)) {
            double tt = json_number_value(tv);
            if (tt > g_last_time) g_last_time = tt;
        }
    }

    json_decref(root);
}

/* ---------- libwebsockets callback ---------- */

static int ws_cb(struct lws *wsi, enum lws_callback_reasons reason,
                 void *user, void *in, size_t len) {
    (void)user;

    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        g_initial_sent = 0;
        lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
        char payload[128];
        int n;
        if (g_want_initial && !g_initial_sent) {
            n = snprintf(payload, sizeof(payload), "{\"initial\":true}");
        } else if (g_last_time > 0) {
            n = snprintf(payload, sizeof(payload),
                         "{\"last_time\":%.0f}", g_last_time);
        } else {
            n = snprintf(payload, sizeof(payload), "{\"initial\":true}");
        }

        unsigned char buf[LWS_PRE + 128];
        memcpy(&buf[LWS_PRE], payload, (size_t)n);
        if (lws_write(wsi, &buf[LWS_PRE], (size_t)n, LWS_WRITE_TEXT) < 0) return -1;
        g_initial_sent = 1;
        g_want_initial = 0;
        break;
    }

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        size_t need = g_msg_len + len + 1;
        if (need > g_msg_cap) {
            size_t nc = g_msg_cap ? g_msg_cap * 2 : 8192;
            while (nc < need) nc *= 2;
            g_msg_buf = realloc(g_msg_buf, nc);
            g_msg_cap = nc;
        }
        memcpy(g_msg_buf + g_msg_len, in, len);
        g_msg_len += len;
        g_msg_buf[g_msg_len] = '\0';

        if (lws_is_final_fragment(wsi)) {
            handle_payload(g_msg_buf, g_msg_len);
            g_msg_len = 0;
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        fprintf(stderr, "holycluster: connection error: %s\n",
                in ? (char *)in : "(n/a)");
        g_wsi = NULL;
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
        g_wsi = NULL;
        break;

    default:
        break;
    }
    return 0;
}

static const struct lws_protocols protocols[] = {
    { "holy-proto", ws_cb, 0, 4096, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};

/* ---------- service thread ---------- */

static int connect_ws(void) {
    struct lws_client_connect_info ci;
    memset(&ci, 0, sizeof(ci));
    ci.context        = g_ctx;
    ci.address        = "holycluster.iarc.org";
    ci.port           = 443;
    ci.path           = "/spots_ws";
    ci.host           = ci.address;
    ci.origin         = "https://holycluster.iarc.org";
    ci.protocol       = protocols[0].name;
    ci.ssl_connection = LCCSCF_USE_SSL;
    ci.pwsi           = &g_wsi;
    return lws_client_connect_via_info(&ci) ? 0 : -1;
}

static void *service_thread(void *arg) {
    (void)arg;
    time_t next_connect = 0;
    int backoff = 1;

    while (ham_tui_running()) {
        if (!g_wsi && time(NULL) >= next_connect) {
            if (connect_ws() != 0) {
                next_connect = time(NULL) + backoff;
                if (backoff < 30) backoff *= 2;
            } else {
                backoff = 1;
            }
        }
        lws_service(g_ctx, 100);
    }
    return NULL;
}

/* ---------- context init ---------- */

static int lws_init_all(void) {
    /* Silence libwebsockets' routine INFO/NOTICE/WARN noise;
       keep only real errors. */
    lws_set_log_level(LLL_ERR, NULL);

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port      = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options   = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    g_ctx = lws_create_context(&info);
    return g_ctx ? 0 : -1;
}

/* ---------- dump mode ---------- */

static int run_dump(void) {
    g_dump_mode = 1;
    if (lws_init_all() != 0) return 1;
    if (connect_ws() != 0) return 1;

    /* Service until we get the initial message, or 10 s timeout. */
    time_t deadline = time(NULL) + 10;
    int got_initial = 0;
    while (time(NULL) < deadline && !got_initial) {
        lws_service(g_ctx, 100);
        if (g_initial_sent && g_last_time > 0) got_initial = 1;
    }
    lws_context_destroy(g_ctx);
    free(g_msg_buf);
    return got_initial ? 0 : 1;
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--dump") == 0) return run_dump();

    ham_config cfg;
    if (ham_config_load(&cfg) != 0) return 1;

    if (lws_init_all() != 0) {
        fprintf(stderr, "holycluster: libwebsockets init failed\n");
        ham_config_free(&cfg);
        return 1;
    }

    ham_tui_init("Country", cfg.fifo_path);

    pthread_t tid;
    pthread_create(&tid, NULL, service_thread, NULL);

    ham_tui_run();
    ham_tui_request_exit();
    pthread_join(tid, NULL);
    ham_tui_cleanup();

    lws_context_destroy(g_ctx);
    free(g_msg_buf);
    ham_config_free(&cfg);
    return 0;
}
