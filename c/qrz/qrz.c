#include "cache.h"
#include "fields.h"
#include "qrzapi.h"
#include "../common/colors.h"
#include "../common/config.h"
#include "../common/locator.h"
#include "../common/paths.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static ham_config  g_cfg;
static ham_cache  *g_cache;
static ham_qrzapi  g_api;
static int         g_oneshot = 0;
static int         g_running = 1;
static int         g_fifo_fd = -1;
static char        g_history[1024];

static char **g_calls;
static size_t g_calls_n;

static void reload_calls(void) {
    if (g_calls) {
        for (size_t i = 0; i < g_calls_n; i++) free(g_calls[i]);
        free(g_calls);
    }
    g_calls = ham_cache_list_calls(g_cache, &g_calls_n);
}

/* ----------------------- display ----------------------- */

static const char *F(const ham_fields *f, const char *k) {
    const char *v = ham_fields_get(f, k);
    return v ? v : "";
}

static void print_kv(const char *label, const char *color, const char *val,
                     int newline, const char *endchar) {
    if (!val || !*val) return;
    printf(C_PLUM C_BOLD "%s" C_RESET "%s%s" C_RESET, label, color, val);
    if (newline) printf("\n");
    else         printf("%s", endchar);
}

static void print_result(const char *callsign, const ham_fields *lookup) {
    const char *call = F(lookup, "call");
    if (!*call) call = callsign;

    const char *aliases = F(lookup, "aliases");
    if (*aliases) {
        printf(C_BLUE "-=" C_TURQ C_BOLD "%s" C_RESET C_BLUE "=-" C_RESET " (%s)\n", call, aliases);
    } else {
        printf(C_BLUE "-=" C_TURQ C_BOLD "%s" C_RESET C_BLUE "=-" C_RESET "\n", call);
    }

    printf(C_PLUM C_BOLD "QTH: " C_RESET);
    print_kv("", C_OLIVE, F(lookup, "fname"),   0, " ");
    print_kv("", C_OLIVE, F(lookup, "name"),    0, ", ");
    print_kv("", C_NAVAJO, F(lookup, "addr1"),  0, ", ");
    print_kv("", C_NAVAJO, F(lookup, "zip"),    0, " ");
    print_kv("", C_NAVAJO, F(lookup, "addr2"),  0, ", ");
    print_kv("", C_NAVAJO, F(lookup, "country"),1, "");

    print_kv("Grid square: ",  C_SEAGREEN, F(lookup, "grid"), 0, " ");
    print_kv("Latitude: ",     C_SEAGREEN, F(lookup, "lat"),  0, " ");
    print_kv("Longitude: ",    C_SEAGREEN, F(lookup, "lon"),  1, "");

    print_kv("CCode: ",        C_SEAGREEN, F(lookup, "ccode"),  0, " ");
    print_kv("CQZone: ",       C_SEAGREEN, F(lookup, "cqzone"), 0, " ");
    print_kv("ITUZone: ",      C_SEAGREEN, F(lookup, "ituzone"),1, "");

    print_kv("QSL: ",          C_NAVAJO,   F(lookup, "qslmgr"), 0, " ");
    print_kv("eQSL: ",         C_NAVAJO,   F(lookup, "eqsl"),   0, " ");
    print_kv("lotw: ",         C_NAVAJO,   F(lookup, "lotw"),   1, "");

    print_kv("E-Mail: ",       C_NAVAJO,   F(lookup, "email"),  1, "");

    const char *lat_s = F(lookup, "lat");
    const char *lon_s = F(lookup, "lon");
    if (*lat_s && *lon_s && (g_cfg.has_qth == 3)) {
        double lat = atof(lat_s), lon = atof(lon_s);
        double h  = ham_heading(g_cfg.qth_lat, g_cfg.qth_lon, lat, lon);
        double lp = ham_longpath(h);
        printf(C_PLUM C_BOLD "Heading: " C_RESET C_NAVAJO "%.1f°" C_RESET, h);
        printf(C_PLUM C_BOLD " Longpath: " C_RESET C_NAVAJO "%.1f°" C_RESET, lp);
        printf(C_PLUM C_BOLD " Bearing: " C_RESET C_NAVAJO "%s" C_RESET "\n",
               ham_bearing_label(h));
    }
    fflush(stdout);
}

/* ----------------------- lookup ----------------------- */

static void do_lookup(const char *raw) {
    while (*raw && isspace((unsigned char)*raw)) raw++;
    if (!*raw) return;

    char *home = ham_qrzapi_homecall(raw);
    if (!home) return;

    ham_fields fields;
    ham_fields_init(&fields);

    int hit = ham_cache_get(g_cache, home, &fields);
    if (hit != 0) {
        if (ham_qrzapi_lookup(&g_api, home, &fields) != 0) {
            printf("Not Found\n");
            ham_fields_free(&fields);
            free(home);
            return;
        }
        ham_cache_set(g_cache, home, &fields);
        reload_calls();
    }

    print_result(home, &fields);
    ham_fields_free(&fields);
    free(home);
}

/* ----------------------- readline ----------------------- */

static const char *g_commands[] = { "lookup", "exit", "quit", NULL };

static char *completer_gen(const char *text, int state) {
    static size_t idx;
    static size_t tlen;
    static int    phase;    /* 0 = commands, 1 = cached calls */

    if (state == 0) {
        idx = 0; tlen = strlen(text); phase = 0;
    }

    if (phase == 0) {
        while (g_commands[idx]) {
            const char *c = g_commands[idx++];
            if (strncasecmp(c, text, tlen) == 0) return strdup(c);
        }
        phase = 1;
        idx = 0;
    }
    while (idx < g_calls_n) {
        const char *c = g_calls[idx++];
        if (strncasecmp(c, text, tlen) == 0) return strdup(c);
    }
    return NULL;
}

static char **completer(const char *text, int start, int end) {
    (void)start; (void)end;
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, completer_gen);
}

static void line_handler(char *line) {
    if (!line) {
        g_running = 0;
        return;
    }
    if (*line) add_history(line);

    char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;

    if (strncasecmp(p, "exit", 4) == 0 || strncasecmp(p, "quit", 4) == 0) {
        g_running = 0;
    } else if (strncasecmp(p, "lookup", 6) == 0 && (p[6] == ' ' || p[6] == '\t')) {
        do_lookup(p + 6);
    } else if (*p) {
        do_lookup(p);
    }

    free(line);
    if (g_oneshot) g_running = 0;
}

/* ----------------------- FIFO ----------------------- */

static void setup_fifo(void) {
    if (mkfifo(g_cfg.fifo_path, 0600) == -1 && errno != EEXIST) {
        fprintf(stderr, "qrz: mkfifo %s: %s\n", g_cfg.fifo_path, strerror(errno));
        return;
    }
    /* O_RDWR keeps the pipe from hitting EOF when writers close. */
    g_fifo_fd = open(g_cfg.fifo_path, O_RDWR | O_NONBLOCK);
    if (g_fifo_fd < 0) {
        fprintf(stderr, "qrz: open %s: %s\n", g_cfg.fifo_path, strerror(errno));
    }
}

static void drain_fifo(void) {
    static char buf[4096];
    static size_t fill = 0;

    while (1) {
        ssize_t n = read(g_fifo_fd, buf + fill, sizeof(buf) - 1 - fill);
        if (n <= 0) break;
        fill += (size_t)n;
        buf[fill] = '\0';
    }

    char *start = buf;
    while (1) {
        char *nl = memchr(start, '\n', fill - (start - buf));
        if (!nl) break;
        *nl = '\0';
        if (*start) {
            printf("\n");
            do_lookup(start);
            rl_forced_update_display();
        }
        start = nl + 1;
    }
    size_t remaining = fill - (start - buf);
    memmove(buf, start, remaining);
    fill = remaining;
}

/* ----------------------- main ----------------------- */

static void on_sigint(int s) { (void)s; g_running = 0; }

int main(int argc, char **argv) {
    if (ham_config_load(&g_cfg) != 0) return 1;
    if (ham_configdir_ensure() != 0) return 1;

    g_cache = ham_cache_open();
    if (!g_cache) return 1;

    if (!g_cfg.qrz_username || !*g_cfg.qrz_username ||
        !g_cfg.qrz_password || !*g_cfg.qrz_password) {
        fprintf(stderr, "qrz: qrz.com.username/password missing from config\n");
    }
    ham_qrzapi_init(&g_api, g_cfg.qrz_username, g_cfg.qrz_password);
    reload_calls();

    /* One-shot: any callsigns on argv get looked up, then exit. */
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            do_lookup(argv[i]);
            if (i + 1 < argc) printf("\n");
        }
        goto cleanup;
    }

    ham_configfile(g_history, sizeof(g_history), "qrz-history");
    read_history(g_history);

    setup_fifo();

    rl_attempted_completion_function = completer;
    rl_callback_handler_install("qrz> ", line_handler);

    signal(SIGINT, on_sigint);

    while (g_running) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        int maxfd = STDIN_FILENO;
        if (g_fifo_fd >= 0) {
            FD_SET(g_fifo_fd, &fds);
            if (g_fifo_fd > maxfd) maxfd = g_fifo_fd;
        }

        int rc = select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (FD_ISSET(STDIN_FILENO, &fds)) rl_callback_read_char();
        if (g_fifo_fd >= 0 && FD_ISSET(g_fifo_fd, &fds)) drain_fifo();
    }

    rl_callback_handler_remove();
    write_history(g_history);

cleanup:
    if (g_fifo_fd >= 0) close(g_fifo_fd);
    if (g_calls) {
        for (size_t i = 0; i < g_calls_n; i++) free(g_calls[i]);
        free(g_calls);
    }
    ham_qrzapi_free(&g_api);
    ham_cache_close(g_cache);
    ham_config_free(&g_cfg);
    return 0;
}
