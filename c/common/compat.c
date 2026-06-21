/* Windows-only implementations of the shims declared in compat.h.
 * On POSIX this translation unit compiles to an empty object. */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ctype.h>
#include <time.h>

#include "compat.h"

unsigned ham_sleep(unsigned seconds) {
    Sleep(seconds * 1000u);
    return 0;
}

char *ham_strcasestr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n &&
               tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

struct tm *ham_gmtime_r(const time_t *timer, struct tm *result) {
    return gmtime_s(result, timer) == 0 ? result : NULL;
}

void ham_win_console_init(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

#else

/* Avoid an empty translation unit (ISO C forbids it). */
typedef int ham_compat_translation_unit_not_empty;

#endif /* _WIN32 */
