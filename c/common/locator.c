#include "locator.h"

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* M_PI is not ISO C; MinGW hides it under a strict -std and some libcs omit it. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const double DEG = M_PI / 180.0;

int ham_latlon_to_locator(double lat, double lon, char out[8]) {
    lon += 180.0;
    lat += 90.0;
    if (lon < 0 || lon >= 360 || lat < 0 || lat >= 180) return -1;

    int field_lon = (int)(lon / 20.0);
    int field_lat = (int)(lat / 10.0);
    lon -= field_lon * 20.0;
    lat -= field_lat * 10.0;

    int sq_lon = (int)(lon / 2.0);
    int sq_lat = (int)lat;
    lon -= sq_lon * 2.0;
    lat -= sq_lat * 1.0;

    int sub_lon = (int)(lon * 12.0);
    int sub_lat = (int)(lat * 24.0);

    out[0] = (char)('A' + field_lon);
    out[1] = (char)('A' + field_lat);
    out[2] = (char)('0' + sq_lon);
    out[3] = (char)('0' + sq_lat);
    out[4] = (char)('a' + sub_lon);
    out[5] = (char)('a' + sub_lat);
    out[6] = '\0';
    return 0;
}

int ham_locator_to_latlon(const char *loc, double *lat, double *lon) {
    if (!loc || strlen(loc) < 4) return -1;
    char a = (char)toupper((unsigned char)loc[0]);
    char b = (char)toupper((unsigned char)loc[1]);
    char c = loc[2];
    char d = loc[3];
    if (a < 'A' || a > 'R' || b < 'A' || b > 'R') return -1;
    if (c < '0' || c > '9' || d < '0' || d > '9') return -1;

    double lo = (a - 'A') * 20.0 + (c - '0') * 2.0;
    double la = (b - 'A') * 10.0 + (d - '0') * 1.0;

    if (strlen(loc) >= 6) {
        char e = (char)tolower((unsigned char)loc[4]);
        char f = (char)tolower((unsigned char)loc[5]);
        if (e >= 'a' && e <= 'x' && f >= 'a' && f <= 'x') {
            lo += (e - 'a') * (2.0 / 24.0) + (1.0 / 24.0);
            la += (f - 'a') * (1.0 / 24.0) + (0.5 / 24.0);
        } else {
            lo += 1.0;
            la += 0.5;
        }
    } else {
        lo += 1.0;
        la += 0.5;
    }

    *lon = lo - 180.0;
    *lat = la - 90.0;
    return 0;
}

double ham_heading(double lat1, double lon1, double lat2, double lon2) {
    double p1 = lat1 * DEG, p2 = lat2 * DEG;
    double dl = (lon2 - lon1) * DEG;
    double y = sin(dl) * cos(p2);
    double x = cos(p1) * sin(p2) - sin(p1) * cos(p2) * cos(dl);
    double h = atan2(y, x) / DEG;
    if (h < 0) h += 360.0;
    return h;
}

double ham_longpath(double heading) {
    double h = heading + 180.0;
    if (h >= 360.0) h -= 360.0;
    return h;
}

const char *ham_bearing_label(double heading) {
    static const char *dirs[16] = {
        "N","NNE","NE","ENE","E","ESE","SE","SSE",
        "S","SSW","SW","WSW","W","WNW","NW","NNW"
    };
    int ix = (int)((heading / 22.5) + 0.5) % 16;
    if (ix < 0) ix += 16;
    return dirs[ix];
}
