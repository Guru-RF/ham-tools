#ifndef HAM_CONFIG_H
#define HAM_CONFIG_H

typedef struct {
    int    verbose;
    double qth_lat;
    double qth_lon;
    int    has_qth;
    char  *qrz_username;
    char  *qrz_password;
    char  *fifo_path;          /* optional override; defaults to configdir/qrz.fifo */
} ham_config;

int  ham_config_load(ham_config *cfg);
void ham_config_free(ham_config *cfg);

#endif
