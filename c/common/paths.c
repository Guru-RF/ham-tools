#include "paths.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "compat.h"

static const char *home_dir(void) {
#ifdef _WIN32
    /* Roaming profile is the idiomatic spot for per-user config on Windows. */
    const char *h = getenv("APPDATA");
    if (h && *h) return h;
    h = getenv("USERPROFILE");
    if (h && *h) return h;
    return ".";
#else
    const char *h = getenv("HOME");
    if (h && *h) return h;
    struct passwd *pw = getpwuid(getuid());
    return pw ? pw->pw_dir : "/";
#endif
}

int ham_configdir(char *out, size_t n) {
#ifdef _WIN32
    /* %APPDATA%\ham-tools — no leading-dot hidden dir on Windows. */
    return snprintf(out, n, "%s/ham-tools", home_dir());
#else
    return snprintf(out, n, "%s/.config/ham-tools", home_dir());
#endif
}

int ham_configdir_ensure(void) {
    char dir[1024];
    ham_configdir(dir, sizeof(dir));
#ifndef _WIN32
    /* Ensure the ~/.config parent exists first; %APPDATA% already exists. */
    char parent[1024];
    snprintf(parent, sizeof(parent), "%s/.config", home_dir());
    ham_mkdir(parent, 0700);
#endif
    if (ham_mkdir(dir, 0700) == -1 && errno != EEXIST) return -1;
    return 0;
}

int ham_configfile(char *out, size_t n, const char *name) {
    char dir[1024];
    ham_configdir(dir, sizeof(dir));
    return snprintf(out, n, "%s/%s", dir, name);
}
