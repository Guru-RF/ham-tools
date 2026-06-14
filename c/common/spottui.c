#include "spottui.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- shared state ---- */

static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

static ham_spot g_spots[HAM_SPOTTUI_MAX];
static int  g_spot_count = 0;
static int  g_selected   = 0;
static int  g_scroll     = 0;

static const char *g_title = "spots";
static char        g_fifo_path[512];
static int         g_fifo_fd = -1;

static int tg_auto_tune = 0;
static int tg_qrz       = 0;
static int tg_follow    = 0;
static int tg_phone     = 0;
static int tg_cw        = 0;
static int tg_digi      = 0;
static int tg_mobile    = 0;
static int tg_portable  = 0;
static int tg_beacon    = 0;
static int tg_qrp       = 0;
static int tg_iota      = 0;
static int tg_sat       = 0;

static char g_last_call[32] = "NONE";
static char g_last_freq[32] = "0.0";
static char g_band[8]       = "--";

static volatile int g_running = 1;

/* ---- band helper ---- */

static const char *khz_to_band(double khz) {
    if (khz >= 1800   && khz <= 2000)   return "160M";
    if (khz >= 3500   && khz <= 4000)   return "80M";
    if (khz >= 5330   && khz <= 5410)   return "60M";
    if (khz >= 7000   && khz <= 7300)   return "40M";
    if (khz >= 10100  && khz <= 10150)  return "30M";
    if (khz >= 14000  && khz <= 14350)  return "20M";
    if (khz >= 18068  && khz <= 18168)  return "17M";
    if (khz >= 21000  && khz <= 21450)  return "15M";
    if (khz >= 24890  && khz <= 24990)  return "12M";
    if (khz >= 28000  && khz <= 29700)  return "10M";
    if (khz >= 50000  && khz <= 54000)  return "6M";
    if (khz >= 144000 && khz <= 148000) return "2M";
    if (khz >= 430000 && khz <= 440000) return "70cm";
    return "--";
}

/* ---- FIFO writer (best-effort) ---- */

static void fifo_open(void) {
    if (g_fifo_path[0] == '\0') return;
    g_fifo_fd = open(g_fifo_path, O_WRONLY | O_NONBLOCK);
}

static void fifo_send(const char *call) {
    if (g_fifo_fd < 0) fifo_open();
    if (g_fifo_fd < 0) return;
    char line[64];
    int n = snprintf(line, sizeof(line), "%s\n", call);
    if (write(g_fifo_fd, line, (size_t)n) < 0 && errno == EPIPE) {
        close(g_fifo_fd);
        g_fifo_fd = -1;
    }
}

/* ---- filters ---- */

static int mode_filter(const ham_spot *s) {
    if (!tg_phone && !tg_cw && !tg_digi) return 1;
    const char *info = s->info;
    const char *mode = s->mode;
    if (tg_phone && (strstr(info, "SSB") || strstr(info, "LSB") ||
                     strstr(info, "USB") || strstr(info, "AM")  ||
                     strcasecmp(mode, "LSB") == 0 ||
                     strcasecmp(mode, "USB") == 0)) return 1;
    if (tg_cw && (strstr(info, "CW") || strcasecmp(mode, "CW") == 0)) return 1;
    if (tg_digi && (strstr(info, "FT8")  || strstr(info, "FT4") ||
                    strstr(info, "RTTY") || strstr(info, "PSK") ||
                    strstr(info, "JT")   ||
                    strcasecmp(mode, "DIGITAL") == 0 ||
                    strcasecmp(mode, "FT8") == 0)) return 1;
    return 0;
}

static int suffix_filter(const ham_spot *s) {
    const char *dx = s->dx;
    int is_m = (strstr(dx, "/M") != NULL);
    int is_p = (strstr(dx, "/P") != NULL);
    int is_b = (strstr(dx, "/B") != NULL);
    if (tg_mobile   && !is_m) return 0;
    if (tg_portable && !is_p) return 0;
    if (tg_beacon   && !is_b) return 0;
    return 1;
}

/* ---- drawing ---- */

static void draw(void) {
    erase();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int header_row  = 0;
    int list_top    = 2;
    int list_bottom = rows - 6;
    int list_height = list_bottom - list_top;
    int toggles_row = list_bottom + 1;
    int info_row    = rows - 1;

    pthread_mutex_lock(&g_mu);

    attron(A_BOLD);
    mvprintw(header_row, 1,
             "%-12s %-9s %-12s %-34s %-6s %s",
             "Spotter", "Freq.", "DX", "Info", "Time", g_title);
    attroff(A_BOLD);
    mvhline(header_row + 1, 0, ACS_HLINE, cols);

    if (g_selected < g_scroll) g_scroll = g_selected;
    if (g_selected >= g_scroll + list_height) g_scroll = g_selected - list_height + 1;
    if (g_scroll < 0) g_scroll = 0;

    int shown = 0;
    for (int i = g_scroll; i < g_spot_count && shown < list_height; i++) {
        ham_spot *s = &g_spots[i];
        if (!mode_filter(s) || !suffix_filter(s)) continue;

        int y = list_top + shown++;
        if (i == g_selected) attron(A_REVERSE);
        int country_w = cols - 76; if (country_w < 0) country_w = 0;
        mvprintw(y, 1, "%-12.12s %-9.9s %-12.12s %-34.34s %-6.6s %.*s",
                 s->spotter, s->freq, s->dx, s->info, s->timez,
                 country_w, s->country);
        if (i == g_selected) attroff(A_REVERSE);
    }

    mvhline(list_bottom, 0, ACS_HLINE, cols);

    const char *row1_names[] = { "auto", "qrz", "follow", "phone", "cw", "digi" };
    int *row1_flags[] = { &tg_auto_tune, &tg_qrz, &tg_follow, &tg_phone, &tg_cw, &tg_digi };
    const char *row2_names[] = { "mobile", "portable", "beacon", "qrp", "sat", "iota" };
    int *row2_flags[] = { &tg_mobile, &tg_portable, &tg_beacon, &tg_qrp, &tg_sat, &tg_iota };

    int cx = 1;
    for (int i = 0; i < 6; i++) {
        mvprintw(toggles_row, cx, "[%c] %s",
                 *row1_flags[i] ? 'X' : ' ', row1_names[i]);
        cx += (int)strlen(row1_names[i]) + 6;
    }
    cx = 1;
    for (int i = 0; i < 6; i++) {
        mvprintw(toggles_row + 1, cx, "[%c] %s",
                 *row2_flags[i] ? 'X' : ' ', row2_names[i]);
        cx += (int)strlen(row2_names[i]) + 6;
    }

    mvhline(info_row - 1, 0, ACS_HLINE, cols);
    attron(A_BOLD);
    mvprintw(info_row, 1,  "Band: %-6s", g_band);
    mvprintw(info_row, 18, "Freq.: %s kHz", g_last_freq);
    mvprintw(info_row, 50, "Call: %s", g_last_call);
    attroff(A_BOLD);

    if (cols >= 40) {
        mvprintw(info_row, cols - 34,
                 "a l f p c d M P B Q s i | q=quit");
    }

    pthread_mutex_unlock(&g_mu);
    refresh();
}

/* ---- actions ---- */

static void tune_selected(void) {
    char call[32] = {0};
    int send = 0;
    pthread_mutex_lock(&g_mu);
    if (g_selected >= 0 && g_selected < g_spot_count) {
        ham_spot *s = &g_spots[g_selected];
        strncpy(g_last_call, s->dx, sizeof(g_last_call) - 1);
        g_last_call[sizeof(g_last_call) - 1] = '\0';
        strncpy(g_last_freq, s->freq, sizeof(g_last_freq) - 1);
        g_last_freq[sizeof(g_last_freq) - 1] = '\0';
        double khz = atof(s->freq);
        strncpy(g_band, khz_to_band(khz), sizeof(g_band) - 1);
        g_band[sizeof(g_band) - 1] = '\0';
        send = tg_qrz;
        strncpy(call, s->dx, sizeof(call) - 1);
    }
    pthread_mutex_unlock(&g_mu);
    if (send && *call) fifo_send(call);
}

static void on_sigint(int s) { (void)s; g_running = 0; }

/* ---- public API ---- */

int ham_tui_init(const char *title, const char *fifo_path) {
    if (title) g_title = title;
    if (fifo_path) {
        strncpy(g_fifo_path, fifo_path, sizeof(g_fifo_path) - 1);
        g_fifo_path[sizeof(g_fifo_path) - 1] = '\0';
    } else {
        g_fifo_path[0] = '\0';
    }
    setlocale(LC_ALL, "");
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(500);
    signal(SIGINT, on_sigint);
    return 0;
}

void ham_tui_set_spots(const ham_spot *arr, int n) {
    if (n > HAM_SPOTTUI_MAX) n = HAM_SPOTTUI_MAX;
    pthread_mutex_lock(&g_mu);
    memcpy(g_spots, arr, sizeof(ham_spot) * (size_t)n);
    g_spot_count = n;
    if (g_selected >= g_spot_count) g_selected = g_spot_count > 0 ? g_spot_count - 1 : 0;

    if (tg_auto_tune && g_spot_count > 0) {
        ham_spot *top = &g_spots[0];
        if (strcmp(top->dx, g_last_call) != 0) {
            strncpy(g_last_call, top->dx, sizeof(g_last_call) - 1);
            g_last_call[sizeof(g_last_call) - 1] = '\0';
            strncpy(g_last_freq, top->freq, sizeof(g_last_freq) - 1);
            g_last_freq[sizeof(g_last_freq) - 1] = '\0';
            double khz = atof(top->freq);
            strncpy(g_band, khz_to_band(khz), sizeof(g_band) - 1);
            g_band[sizeof(g_band) - 1] = '\0';
            if (tg_qrz) {
                char c[32];
                strncpy(c, top->dx, sizeof(c) - 1);
                c[sizeof(c) - 1] = '\0';
                pthread_mutex_unlock(&g_mu);
                fifo_send(c);
                return;
            }
        }
    }
    pthread_mutex_unlock(&g_mu);
}

void ham_tui_add_spot(const ham_spot *s) {
    pthread_mutex_lock(&g_mu);
    int n = g_spot_count < HAM_SPOTTUI_MAX ? g_spot_count : HAM_SPOTTUI_MAX - 1;
    if (n > 0) memmove(&g_spots[1], &g_spots[0], sizeof(ham_spot) * (size_t)n);
    g_spots[0] = *s;
    if (g_spot_count < HAM_SPOTTUI_MAX) g_spot_count++;
    if (g_selected > 0 && g_selected + 1 < g_spot_count) g_selected++;

    int send_qrz = 0;
    char c[32] = {0};
    if (tg_auto_tune) {
        strncpy(g_last_call, s->dx, sizeof(g_last_call) - 1);
        g_last_call[sizeof(g_last_call) - 1] = '\0';
        strncpy(g_last_freq, s->freq, sizeof(g_last_freq) - 1);
        g_last_freq[sizeof(g_last_freq) - 1] = '\0';
        double khz = atof(s->freq);
        strncpy(g_band, khz_to_band(khz), sizeof(g_band) - 1);
        g_band[sizeof(g_band) - 1] = '\0';
        send_qrz = tg_qrz;
        strncpy(c, s->dx, sizeof(c) - 1);
    }
    pthread_mutex_unlock(&g_mu);
    if (send_qrz && *c) fifo_send(c);
}

void ham_tui_request_exit(void) { g_running = 0; }
int  ham_tui_running(void)      { return g_running; }

void ham_tui_run(void) {
    while (g_running) {
        draw();
        int ch = getch();
        if (ch == ERR) continue;

        switch (ch) {
        case 'q': case 27: case 3:
            g_running = 0; break;
        case KEY_UP:
            pthread_mutex_lock(&g_mu);
            if (g_selected > 0) g_selected--;
            pthread_mutex_unlock(&g_mu);
            break;
        case KEY_DOWN:
            pthread_mutex_lock(&g_mu);
            if (g_selected + 1 < g_spot_count) g_selected++;
            pthread_mutex_unlock(&g_mu);
            break;
        case KEY_NPAGE:
            pthread_mutex_lock(&g_mu);
            g_selected += 10;
            if (g_selected >= g_spot_count) g_selected = g_spot_count - 1;
            if (g_selected < 0) g_selected = 0;
            pthread_mutex_unlock(&g_mu);
            break;
        case KEY_PPAGE:
            pthread_mutex_lock(&g_mu);
            g_selected -= 10;
            if (g_selected < 0) g_selected = 0;
            pthread_mutex_unlock(&g_mu);
            break;
        case '\n': case '\r': case ' ': case 't':
            tune_selected(); break;
        case 'a': tg_auto_tune = !tg_auto_tune;
                  if (tg_auto_tune) tune_selected(); break;
        case 'l': tg_qrz   = !tg_qrz;   break;
        case 'f': tg_follow= !tg_follow;break;
        case 'p': tg_phone = !tg_phone; break;
        case 'c': tg_cw    = !tg_cw;    break;
        case 'd': tg_digi  = !tg_digi;  break;
        case 'M':
            tg_mobile = !tg_mobile;
            if (tg_mobile) { tg_portable = 0; tg_beacon = 0; } break;
        case 'P':
            tg_portable = !tg_portable;
            if (tg_portable) { tg_mobile = 0; tg_beacon = 0; } break;
        case 'B':
            tg_beacon = !tg_beacon;
            if (tg_beacon) { tg_mobile = 0; tg_portable = 0; } break;
        case 'Q': tg_qrp  = !tg_qrp;  break;
        case 's': tg_sat  = !tg_sat;  break;
        case 'i': tg_iota = !tg_iota; break;
        }
    }
}

void ham_tui_cleanup(void) {
    endwin();
    if (g_fifo_fd >= 0) close(g_fifo_fd);
}
