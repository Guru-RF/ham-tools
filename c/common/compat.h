#ifndef HAM_COMPAT_H
#define HAM_COMPAT_H

/* Portability shims so the ham-tools sources build as native Windows
 * binaries under MSYS2 MinGW-w64 (UCRT64 for x86_64, CLANGARM64 for
 * Windows-on-ARM) in addition to POSIX (Linux, macOS).
 *
 * Include this LAST, after all system/library headers, in every .c file:
 * the Windows branch defines a few function-like macros (strdup, sleep,
 * gmtime_r, strcasestr, strtok_r) that we only want applied to the
 * project's own code, not to third-party headers.
 */

#ifdef _WIN32

/* Expose M_PI from <math.h> even when a strict -std=cNN hides it. */
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES 1
#endif

#include <direct.h>   /* _mkdir   */
#include <string.h>   /* _stricmp, _strnicmp, _strdup, strtok_s */
#include <time.h>     /* time_t, struct tm */

/* POSIX/BSD spellings -> Win32 CRT equivalents. */
#ifndef strcasecmp
#define strcasecmp  _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#ifndef strdup
#define strdup      _strdup
#endif
#ifndef strtok_r
#define strtok_r    strtok_s
#endif

/* mkdir(path, mode): Win32 _mkdir() takes no mode argument. */
#define ham_mkdir(path, mode) _mkdir(path)

/* sleep(seconds): POSIX. Implemented over Win32 Sleep() (milliseconds). */
unsigned ham_sleep(unsigned seconds);
#ifndef sleep
#define sleep(s) ham_sleep((unsigned)(s))
#endif

/* strcasestr(): GNU extension, absent from the MinGW CRT. */
char *ham_strcasestr(const char *haystack, const char *needle);
#ifndef strcasestr
#define strcasestr ham_strcasestr
#endif

/* gmtime_r(&t,&tm): MinGW ships gmtime_s(&tm,&t) with reversed args. */
struct tm *ham_gmtime_r(const time_t *timer, struct tm *result);
#ifndef gmtime_r
#define gmtime_r ham_gmtime_r
#endif

/* Put the Windows console into UTF-8 so ACS line-drawing and the degree
 * sign render correctly; no-op on POSIX. */
void ham_win_console_init(void);

#else  /* POSIX */

#include <strings.h>   /* strcasecmp, strncasecmp        */
#include <sys/stat.h>  /* mkdir                          */
#include <unistd.h>    /* sleep                          */

#define ham_mkdir(path, mode) mkdir((path), (mode))
#define ham_win_console_init() ((void)0)

#endif /* _WIN32 */

#endif /* HAM_COMPAT_H */
