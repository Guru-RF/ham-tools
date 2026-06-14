#include "paths.h"

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    if (h && *h) return h;
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/";
}

int ham_configdir(char *out, size_t n) {
    return snprintf(out, n, "%s/.config/ham-tools", home_dir());
}

int ham_configdir_ensure(void) {
    char dir[1024];
    ham_configdir(dir, sizeof(dir));
    char parent[1024];
    snprintf(parent, sizeof(parent), "%s/.config", home_dir());
    mkdir(parent, 0700);
    if (mkdir(dir, 0700) == -1 && errno != EEXIST) return -1;
    return 0;
}

int ham_configfile(char *out, size_t n, const char *name) {
    char dir[1024];
    ham_configdir(dir, sizeof(dir));
    return snprintf(out, n, "%s/%s", dir, name);
}
