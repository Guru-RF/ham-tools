#include "../common/config.h"
#include "../common/http.h"
#include "../common/spot.h"
#include "../common/spottui.h"

#include <jansson.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *SPOTS_URL = "https://dxheat.com/source/spots/?a=100";

/* ---------- parse one spot JSON object ---------- */

static const char *js(json_t *obj, const char *k) {
    json_t *v = json_object_get(obj, k);
    if (!v) return "";
    if (json_is_string(v))  return json_string_value(v);
    return "";
}

static int parse_spot(json_t *obj, ham_spot *s) {
    memset(s, 0, sizeof(*s));

    strncpy(s->spotter, js(obj, "Spotter"), sizeof(s->spotter) - 1);
    strncpy(s->freq,    js(obj, "Frequency"), sizeof(s->freq) - 1);
    strncpy(s->dx,      js(obj, "DXCall"), sizeof(s->dx) - 1);
    strncpy(s->info,    js(obj, "Comment"), sizeof(s->info) - 1);
    strncpy(s->mode,    js(obj, "Mode"), sizeof(s->mode) - 1);

    const char *t = js(obj, "Time");
    strncpy(s->timez, t, sizeof(s->timez) - 1);

    const char *flag = js(obj, "Flag");  /* ISO-like country code */
    strncpy(s->country, flag, sizeof(s->country) - 1);

    if (!*s->spotter || !*s->freq || !*s->dx) return -1;
    return 0;
}

/* ---------- producer ---------- */

static void *fetch_thread(void *arg) {
    (void)arg;
    while (ham_tui_running()) {
        ham_buf body = {0};
        if (ham_http_get(SPOTS_URL, "ham-tools-dxheat-c", &body) == 0) {
            json_error_t jerr;
            json_t *root = json_loads(body.data, 0, &jerr);
            if (root && json_is_array(root)) {
                ham_spot arr[HAM_SPOTTUI_MAX];
                int n = 0;
                size_t count = json_array_size(root);
                for (size_t i = 0; i < count && n < HAM_SPOTTUI_MAX; i++) {
                    ham_spot s;
                    if (parse_spot(json_array_get(root, i), &s) == 0) arr[n++] = s;
                }
                ham_tui_set_spots(arr, n);
            }
            if (root) json_decref(root);
        }
        ham_buf_free(&body);
        for (int i = 0; i < 15 && ham_tui_running(); i++) sleep(1);
    }
    return NULL;
}

/* ---------- dump mode ---------- */

static int run_dump(void) {
    ham_buf body = {0};
    if (ham_http_get(SPOTS_URL, "ham-tools-dxheat-c", &body) != 0) {
        fprintf(stderr, "fetch failed\n"); return 1;
    }
    json_error_t jerr;
    json_t *root = json_loads(body.data, 0, &jerr);
    ham_buf_free(&body);
    if (!root || !json_is_array(root)) {
        fprintf(stderr, "bad JSON\n");
        if (root) json_decref(root);
        return 1;
    }

    int n = 0;
    size_t count = json_array_size(root);
    for (size_t i = 0; i < count; i++) {
        ham_spot s;
        if (parse_spot(json_array_get(root, i), &s) != 0) continue;
        printf("%-10s | %-8s | %-12s | %-8s | %-30s | %-5s | %s\n",
               s.spotter, s.freq, s.dx, s.mode, s.info, s.timez, s.country);
        n++;
    }
    json_decref(root);
    fprintf(stderr, "parsed %d spots\n", n);
    return 0;
}

/* ---------- main ---------- */

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "--dump") == 0) return run_dump();

    ham_config cfg;
    if (ham_config_load(&cfg) != 0) return 1;

    ham_tui_init("Flag", cfg.fifo_path);

    pthread_t tid;
    pthread_create(&tid, NULL, fetch_thread, NULL);

    ham_tui_run();
    ham_tui_request_exit();
    pthread_join(tid, NULL);
    ham_tui_cleanup();
    ham_config_free(&cfg);
    return 0;
}
