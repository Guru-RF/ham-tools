#ifndef HAM_LOCATOR_H
#define HAM_LOCATOR_H

int   ham_latlon_to_locator(double lat, double lon, char out[8]);
int   ham_locator_to_latlon(const char *loc, double *lat, double *lon);
double ham_heading(double lat1, double lon1, double lat2, double lon2);
double ham_longpath(double heading);
const char *ham_bearing_label(double heading);

#endif
