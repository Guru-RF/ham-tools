#include "config.h"
#include "paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include "compat.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static int is_truthy(const char *s) {
    if (!s) return 0;
    return strcasecmp(s, "true") == 0 || strcasecmp(s, "yes") == 0 ||
           strcasecmp(s, "on")   == 0 || strcmp(s, "1") == 0;
}

/* Walks a YAML mapping, setting fields by dotted key path.
   Stack holds the current nested path, e.g. "qth.latitude". */
static void assign(ham_config *cfg, const char *path, const char *value) {
    if (!value) return;
    if (strcmp(path, "verbose") == 0) {
        cfg->verbose = is_truthy(value);
    } else if (strcmp(path, "qth.latitude") == 0) {
        cfg->qth_lat = strtod(value, NULL);
        cfg->has_qth |= 1;
    } else if (strcmp(path, "qth.longitude") == 0) {
        cfg->qth_lon = strtod(value, NULL);
        cfg->has_qth |= 2;
    } else if (strcmp(path, "qrz.com.username") == 0) {
        free(cfg->qrz_username);
        cfg->qrz_username = xstrdup(value);
    } else if (strcmp(path, "qrz.com.password") == 0) {
        free(cfg->qrz_password);
        cfg->qrz_password = xstrdup(value);
    } else if (strcmp(path, "fifo") == 0) {
        free(cfg->fifo_path);
        cfg->fifo_path = xstrdup(value);
    }
}

/* Depth-first walk of the YAML event stream.
   We only care about scalar leaves inside nested mappings. */
static int parse_events(yaml_parser_t *p, ham_config *cfg) {
    /* path components joined with '.' */
    char path[512] = {0};
    /* stack of path-lengths so we can pop */
    size_t depth[32] = {0};
    int    di = 0;
    /* in a mapping, next scalar is a key unless we're expecting a value */
    int    expect_key[32] = {0};
    int    xi = 0;
    char   pending_key[128] = {0};

    yaml_event_t ev;
    int in_mapping = 0;

    while (1) {
        if (!yaml_parser_parse(p, &ev)) return -1;
        int done = (ev.type == YAML_STREAM_END_EVENT);

        switch (ev.type) {
        case YAML_MAPPING_START_EVENT:
            if (in_mapping && pending_key[0]) {
                /* descend: append pending_key */
                depth[di++] = strlen(path);
                if (path[0]) strncat(path, ".", sizeof(path)-strlen(path)-1);
                strncat(path, pending_key, sizeof(path)-strlen(path)-1);
                pending_key[0] = '\0';
            } else {
                depth[di++] = strlen(path);
            }
            expect_key[xi++] = 1;
            in_mapping = 1;
            break;

        case YAML_MAPPING_END_EVENT:
            if (di > 0) { path[depth[--di]] = '\0'; }
            if (xi > 0) xi--;
            in_mapping = (xi > 0);
            break;

        case YAML_SEQUENCE_START_EVENT:
            /* we don't consume sequence values; skip until matching end */
            {
                int lvl = 1;
                yaml_event_t sub;
                while (lvl > 0) {
                    if (!yaml_parser_parse(p, &sub)) { yaml_event_delete(&ev); return -1; }
                    if (sub.type == YAML_SEQUENCE_START_EVENT ||
                        sub.type == YAML_MAPPING_START_EVENT) lvl++;
                    if (sub.type == YAML_SEQUENCE_END_EVENT ||
                        sub.type == YAML_MAPPING_END_EVENT) lvl--;
                    yaml_event_delete(&sub);
                }
                if (in_mapping) expect_key[xi-1] = 1;
                pending_key[0] = '\0';
            }
            break;

        case YAML_SCALAR_EVENT: {
            const char *v = (const char *)ev.data.scalar.value;
            if (in_mapping && xi > 0 && expect_key[xi-1]) {
                strncpy(pending_key, v, sizeof(pending_key)-1);
                pending_key[sizeof(pending_key)-1] = '\0';
                expect_key[xi-1] = 0;
            } else {
                /* scalar value for pending_key */
                char full[640];
                if (path[0])
                    snprintf(full, sizeof(full), "%s.%s", path, pending_key);
                else
                    snprintf(full, sizeof(full), "%s", pending_key);
                assign(cfg, full, v);
                pending_key[0] = '\0';
                if (in_mapping) expect_key[xi-1] = 1;
            }
            break;
        }

        default: break;
        }

        yaml_event_delete(&ev);
        if (done) break;
    }
    return 0;
}

int ham_config_load(ham_config *cfg) {
    memset(cfg, 0, sizeof(*cfg));

    char path[1024];
    ham_configfile(path, sizeof(path), "config.yaml");

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "config: cannot open %s\n", path);
        return -1;
    }

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        return -1;
    }
    yaml_parser_set_input_file(&parser, f);

    int rc = parse_events(&parser, cfg);
    yaml_parser_delete(&parser);
    fclose(f);

    if (rc != 0) {
        fprintf(stderr, "config: YAML parse error in %s\n", path);
        return -1;
    }

    if (!cfg->fifo_path) {
        char p[1024];
        ham_configfile(p, sizeof(p), "qrz.fifo");
        cfg->fifo_path = xstrdup(p);
    }

    return 0;
}

void ham_config_free(ham_config *cfg) {
    if (!cfg) return;
    free(cfg->qrz_username);
    free(cfg->qrz_password);
    free(cfg->fifo_path);
    memset(cfg, 0, sizeof(*cfg));
}
