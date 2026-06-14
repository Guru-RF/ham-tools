#ifndef HAM_PATHS_H
#define HAM_PATHS_H

#include <stddef.h>

int  ham_configdir(char *out, size_t n);
int  ham_configdir_ensure(void);
int  ham_configfile(char *out, size_t n, const char *name);

#endif
