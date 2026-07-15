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

/* ham_config_load return codes. */
#define HAM_CONFIG_OK       0   /* loaded an existing config                 */
#define HAM_CONFIG_ERROR  (-1)  /* real error (unreadable / parse failure)   */
#define HAM_CONFIG_CREATED  1   /* no config existed: a starter template was  \
                                   written; the caller should tell the user   \
                                   and exit successfully                       */

int  ham_config_load(ham_config *cfg);
void ham_config_free(ham_config *cfg);

#endif
