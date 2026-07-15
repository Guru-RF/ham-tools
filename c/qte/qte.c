#include "../common/colors.h"
#include "../common/config.h"
#include "../common/http.h"
#include "../common/locator.h"

#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr, "qte <address>\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(); return 1; }

    /* Join argv into a single address string */
    size_t n = 1;
    for (int i = 1; i < argc; i++) n += strlen(argv[i]) + 1;
    char *query = malloc(n);
    if (!query) return 1;
    query[0] = '\0';
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(query, " ");
        strcat(query, argv[i]);
    }

    ham_config cfg;
    int cfg_rc = ham_config_load(&cfg);
    if (cfg_rc == HAM_CONFIG_CREATED) { free(query); return 0; }
    if (cfg_rc != 0) { free(query); return 1; }
    if (cfg.has_qth != 3) {
        fprintf(stderr, "qte: qth.latitude / qth.longitude missing from config\n");
        ham_config_free(&cfg); free(query);
        return 1;
    }

    char *enc = ham_url_encode(query);
    if (!enc) { ham_config_free(&cfg); free(query); return 1; }

    char url[2048];
    snprintf(url, sizeof(url),
             "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1",
             enc);
    free(enc);

    ham_buf body = {0};
    if (ham_http_get(url, "ham-tools/qte (github.com/Guru-RF/ham-tools)", &body) != 0) {
        fprintf(stderr, "qte: HTTP request failed\n");
        ham_config_free(&cfg); free(query);
        return 1;
    }

    json_error_t jerr;
    json_t *root = json_loads(body.data, 0, &jerr);
    ham_buf_free(&body);
    if (!root || !json_is_array(root) || json_array_size(root) == 0) {
        printf("qte <address>\n");
        if (root) json_decref(root);
        ham_config_free(&cfg); free(query);
        return 1;
    }

    json_t *o = json_array_get(root, 0);
    json_t *j_lat  = json_object_get(o, "lat");
    json_t *j_lon  = json_object_get(o, "lon");
    json_t *j_addr = json_object_get(o, "display_name");

    double lat = j_lat ? atof(json_string_value(j_lat)) : 0.0;
    double lon = j_lon ? atof(json_string_value(j_lon)) : 0.0;
    const char *addr = j_addr ? json_string_value(j_addr) : "";

    char grid[8];
    ham_latlon_to_locator(lat, lon, grid);
    double heading  = ham_heading(cfg.qth_lat, cfg.qth_lon, lat, lon);
    double longpath = ham_longpath(heading);

    printf(C_BLUE "-=" C_TURQ C_BOLD "QTE: Bearing lookup" C_RESET C_BLUE "=-" C_RESET "\n");

    printf(C_MAGENTA C_BOLD "Address: " C_RESET C_SEAGREEN "%s\n" C_RESET, addr);
    printf(C_MAGENTA C_BOLD "Latitude: " C_RESET C_SEAGREEN "%.1f°" C_RESET, lat);
    printf(C_MAGENTA C_BOLD " Longitude: " C_RESET C_SEAGREEN "%.1f°" C_RESET, lon);
    printf(C_MAGENTA C_BOLD " Grid square: " C_RESET C_SEAGREEN "%s\n" C_RESET, grid);
    printf("\n");
    printf(C_MAGENTA C_BOLD "Heading: " C_RESET C_NAVAJO "%.1f°" C_RESET, heading);
    printf(C_MAGENTA C_BOLD " Longpath: " C_RESET C_NAVAJO "%.1f°" C_RESET, longpath);
    printf(C_MAGENTA C_BOLD " Bearing: " C_RESET C_NAVAJO "%s" C_RESET "\n",
           ham_bearing_label(heading));

    json_decref(root);
    ham_config_free(&cfg);
    free(query);
    return 0;
}
