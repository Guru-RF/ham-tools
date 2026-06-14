#ifndef HAM_QRZAPI_H
#define HAM_QRZAPI_H

#include "fields.h"

typedef struct {
    char *session_key;
    char *username;
    char *password;
} ham_qrzapi;

void ham_qrzapi_init(ham_qrzapi *a, const char *user, const char *pwd);
void ham_qrzapi_free(ham_qrzapi *a);

/* Returns 0 on success, -1 on failure. On success fills `out` with fields. */
int ham_qrzapi_lookup(ham_qrzapi *a, const char *call, ham_fields *out);

/* Reduce a fancy callsign (e.g. "F/ON3URE/P") to the home call ("ON3URE").
   Returns a malloc()ed string. */
char *ham_qrzapi_homecall(const char *call);

#endif
